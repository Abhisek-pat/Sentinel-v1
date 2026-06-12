import json
import os
import re
import shutil
import sqlite3
import subprocess
import time
from pathlib import Path
from typing import Any

from fastapi import FastAPI, Query
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

KPI_DIR = Path(os.environ.get("SENTINEL_KPI_DIR", "/var/lib/sentinel/kpi"))
DATABASE_PATH = Path(
    os.environ.get("SENTINEL_KPI_DATABASE", "/var/lib/sentinel/dashboard/kpi.db")
)

app = FastAPI(title="Sentinel KPI API")
STATIC_DIR = Path(__file__).parent / "static"
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")


def connect() -> sqlite3.Connection:
    DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(DATABASE_PATH)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA journal_mode=WAL")
    return connection


def initialize_database() -> None:
    with connect() as connection:
        connection.executescript(
            """
            CREATE TABLE IF NOT EXISTS telemetry (
                timestamp_ms INTEGER PRIMARY KEY,
                capture_fps REAL NOT NULL,
                detection_fps REAL NOT NULL,
                preprocess_ms REAL NOT NULL,
                inference_ms REAL NOT NULL,
                postprocess_ms REAL NOT NULL,
                persons INTEGER NOT NULL,
                rtsp_reconnects INTEGER NOT NULL,
                last_frame_age_ms INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp_ms INTEGER NOT NULL,
                category TEXT NOT NULL,
                message TEXT NOT NULL,
                UNIQUE(timestamp_ms, category, message)
            );
            """
        )


def ingest_jsonl(path: Path, insert_sql: str, fields: tuple[str, ...]) -> int:
    if not path.exists():
        return 0

    rows: list[tuple[Any, ...]] = []
    with path.open("r", encoding="utf-8") as source:
        for line in source:
            try:
                record = json.loads(line)
                rows.append(tuple(record[field] for field in fields))
            except (json.JSONDecodeError, KeyError, TypeError):
                continue

    if not rows:
        return 0

    with connect() as connection:
        before = connection.total_changes
        connection.executemany(insert_sql, rows)
        return connection.total_changes - before


def ingest() -> dict[str, int]:
    telemetry_count = ingest_jsonl(
        KPI_DIR / "telemetry.jsonl",
        """
        INSERT OR IGNORE INTO telemetry (
            timestamp_ms, capture_fps, detection_fps, preprocess_ms,
            inference_ms, postprocess_ms, persons, rtsp_reconnects,
            last_frame_age_ms
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            "timestamp_ms",
            "capture_fps",
            "detection_fps",
            "preprocess_ms",
            "inference_ms",
            "postprocess_ms",
            "persons",
            "rtsp_reconnects",
            "last_frame_age_ms",
        ),
    )
    event_count = ingest_jsonl(
        KPI_DIR / "events.jsonl",
        "INSERT OR IGNORE INTO events (timestamp_ms, category, message) VALUES (?, ?, ?)",
        ("timestamp_ms", "category", "message"),
    )
    return {"telemetry": telemetry_count, "events": event_count}


def read_meminfo() -> dict[str, int]:
    values: dict[str, int] = {}
    try:
        with Path("/proc/meminfo").open("r", encoding="utf-8") as source:
            for line in source:
                key, raw_value = line.split(":", 1)
                values[key] = int(raw_value.strip().split()[0]) * 1024
    except (OSError, ValueError):
        pass
    return values


def read_temperature_c() -> float | None:
    try:
        return round(
            int(Path("/sys/class/thermal/thermal_zone0/temp").read_text().strip()) / 1000,
            1,
        )
    except (OSError, ValueError):
        return None


def read_throttled() -> str | None:
    try:
        result = subprocess.run(
            ["vcgencmd", "get_throttled"],
            capture_output=True,
            check=False,
            text=True,
            timeout=2,
        )
        value = result.stdout.strip()
        return value.split("=", 1)[1] if "=" in value else None
    except (OSError, subprocess.TimeoutExpired):
        return None


def device_health() -> dict[str, Any]:
    memory = read_meminfo()
    total = memory.get("MemTotal", 0)
    available = memory.get("MemAvailable", 0)
    swap_total = memory.get("SwapTotal", 0)
    swap_free = memory.get("SwapFree", 0)
    disk = shutil.disk_usage("/var/lib/sentinel" if Path("/var/lib/sentinel").exists() else "/")

    return {
        "temperature_c": read_temperature_c(),
        "throttled": read_throttled(),
        "load_average": list(os.getloadavg()) if hasattr(os, "getloadavg") else None,
        "uptime_sec": round(time.monotonic()),
        "memory_total_bytes": total,
        "memory_available_bytes": available,
        "memory_used_percent": round(((total - available) / total) * 100, 1) if total else None,
        "swap_used_bytes": swap_total - swap_free,
        "disk_total_bytes": disk.total,
        "disk_free_bytes": disk.free,
        "disk_used_percent": round((disk.used / disk.total) * 100, 1) if disk.total else None,
    }


def event_quality(connection: sqlite3.Connection) -> dict[str, Any]:
    rows = connection.execute(
        "SELECT timestamp_ms, category, message FROM events ORDER BY timestamp_ms DESC LIMIT 5000"
    ).fetchall()
    track_ids: set[int] = set()
    short_zone_exits = 0
    scene_entries = 0
    scene_exits = 0
    loitering_events = 0
    clip_events = 0

    for row in rows:
        message = row["message"]
        match = re.search(r"Track (\d+)", message)
        if match:
            track_ids.add(int(match.group(1)))

        if row["category"] == "scene":
            scene_entries += "entered scene" in message
            scene_exits += "exited scene" in message
        elif row["category"] == "zone":
            loitering_events += "loitering" in message
            exit_match = re.search(r"exited .* after ([0-9.]+)s", message)
            if exit_match and float(exit_match.group(1)) < 2.0:
                short_zone_exits += 1
        elif row["category"] == "clip":
            clip_events += 1

    return {
        "unique_tracks": len(track_ids),
        "scene_entries": scene_entries,
        "scene_exits": scene_exits,
        "short_zone_exits": short_zone_exits,
        "loitering_events": loitering_events,
        "clip_events": clip_events,
        "events_analyzed": len(rows),
    }


@app.on_event("startup")
def startup() -> None:
    initialize_database()
    ingest()


@app.get("/health")
def health() -> dict[str, Any]:
    ingest()
    with connect() as connection:
        latest = connection.execute(
            "SELECT timestamp_ms FROM telemetry ORDER BY timestamp_ms DESC LIMIT 1"
        ).fetchone()
    return {
        "status": "ok",
        "latest_telemetry_ms": latest["timestamp_ms"] if latest else None,
    }


@app.get("/")
def dashboard() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


@app.get("/api/device")
def device() -> dict[str, Any]:
    return device_health()


@app.get("/api/kpis/latest")
def latest_kpis() -> dict[str, Any]:
    ingest()
    with connect() as connection:
        row = connection.execute(
            "SELECT * FROM telemetry ORDER BY timestamp_ms DESC LIMIT 1"
        ).fetchone()
    return dict(row) if row else {}


@app.get("/api/kpis/history")
def kpi_history(limit: int = Query(default=120, ge=1, le=5000)) -> list[dict[str, Any]]:
    ingest()
    with connect() as connection:
        rows = connection.execute(
            "SELECT * FROM telemetry ORDER BY timestamp_ms DESC LIMIT ?", (limit,)
        ).fetchall()
    return [dict(row) for row in reversed(rows)]


@app.get("/api/events")
def events(limit: int = Query(default=100, ge=1, le=1000)) -> list[dict[str, Any]]:
    ingest()
    with connect() as connection:
        rows = connection.execute(
            "SELECT * FROM events ORDER BY timestamp_ms DESC, id DESC LIMIT ?", (limit,)
        ).fetchall()
    return [dict(row) for row in rows]


@app.get("/api/summary")
def summary() -> dict[str, Any]:
    ingest()
    with connect() as connection:
        telemetry = connection.execute(
            """
            SELECT
                COUNT(*) AS samples,
                AVG(capture_fps) AS capture_fps_avg,
                AVG(detection_fps) AS detection_fps_avg,
                AVG(inference_ms) AS inference_ms_avg,
                MAX(rtsp_reconnects) AS rtsp_reconnects,
                MAX(last_frame_age_ms) AS last_frame_age_ms_max
            FROM telemetry
            """
        ).fetchone()
        categories = connection.execute(
            "SELECT category, COUNT(*) AS count FROM events GROUP BY category"
        ).fetchall()
        quality = event_quality(connection)

    result = dict(telemetry)
    result["events"] = {row["category"]: row["count"] for row in categories}
    result["event_quality"] = quality
    return result


@app.get("/api/dashboard")
def dashboard_data() -> dict[str, Any]:
    ingest()
    with connect() as connection:
        latest = connection.execute(
            "SELECT * FROM telemetry ORDER BY timestamp_ms DESC LIMIT 1"
        ).fetchone()
        history = connection.execute(
            "SELECT * FROM telemetry ORDER BY timestamp_ms DESC LIMIT 120"
        ).fetchall()
        recent_events = connection.execute(
            "SELECT * FROM events ORDER BY timestamp_ms DESC, id DESC LIMIT 30"
        ).fetchall()
        quality = event_quality(connection)

    latest_dict = dict(latest) if latest else {}
    latest_timestamp = latest_dict.get("timestamp_ms")
    telemetry_age_sec = (
        round((time.time() * 1000 - latest_timestamp) / 1000, 1)
        if latest_timestamp
        else None
    )

    return {
        "latest": latest_dict,
        "telemetry_age_sec": telemetry_age_sec,
        "history": [dict(row) for row in reversed(history)],
        "events": [dict(row) for row in recent_events],
        "event_quality": quality,
        "device": device_health(),
    }
