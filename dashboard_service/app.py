import json
import os
import re
import shutil
import sqlite3
import subprocess
import time
from pathlib import Path
from typing import Annotated, Any

from fastapi import FastAPI, Query
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

KPI_DIR = Path(os.environ.get("SENTINEL_KPI_DIR", "/var/lib/sentinel/kpi"))
DATABASE_PATH = Path(
    os.environ.get("SENTINEL_KPI_DATABASE", "/var/lib/sentinel/dashboard/kpi.db")
)
RETENTION_DAYS = max(1, int(os.environ.get("SENTINEL_KPI_RETENTION_DAYS", "7")))
ALERT_CAPTURE_FPS_MIN = float(os.environ.get("SENTINEL_ALERT_CAPTURE_FPS_MIN", "12"))
ALERT_CAPTURE_DELIVERY_PERCENT_MIN = float(
    os.environ.get("SENTINEL_ALERT_CAPTURE_DELIVERY_PERCENT_MIN", "70")
)
ALERT_INFERENCE_MS_MAX = float(os.environ.get("SENTINEL_ALERT_INFERENCE_MS_MAX", "100"))
ALERT_TEMPERATURE_C_MAX = float(os.environ.get("SENTINEL_ALERT_TEMPERATURE_C_MAX", "75"))
ALERT_DISK_USED_PERCENT_MAX = float(
    os.environ.get("SENTINEL_ALERT_DISK_USED_PERCENT_MAX", "85")
)
ALERT_SHORT_ZONE_EXIT_RATIO_MAX = float(
    os.environ.get("SENTINEL_ALERT_SHORT_ZONE_EXIT_RATIO_MAX", "0.5")
)
ALERT_TELEMETRY_AGE_SEC_MAX = float(
    os.environ.get("SENTINEL_ALERT_TELEMETRY_AGE_SEC_MAX", "90")
)
ALERT_REASONING_PENDING_MAX = int(os.environ.get("SENTINEL_ALERT_REASONING_PENDING_MAX", "3"))
ALERT_REASONING_FAILURE_RATIO_MAX = float(
    os.environ.get("SENTINEL_ALERT_REASONING_FAILURE_RATIO_MAX", "0.25")
)
JSONL_MAX_BYTES = max(
    1024 * 1024,
    int(os.environ.get("SENTINEL_KPI_JSONL_MAX_MB", "25")) * 1024 * 1024,
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
                source_fps REAL NOT NULL DEFAULT 0,
                capture_fps REAL NOT NULL,
                capture_delivery_percent REAL NOT NULL DEFAULT 0,
                detection_fps REAL NOT NULL,
                preprocess_ms REAL NOT NULL,
                inference_ms REAL NOT NULL,
                postprocess_ms REAL NOT NULL,
                persons INTEGER NOT NULL,
                rtsp_reconnects INTEGER NOT NULL,
                last_frame_age_ms INTEGER NOT NULL,
                capture_read_avg_ms REAL NOT NULL DEFAULT 0,
                capture_read_max_ms REAL NOT NULL DEFAULT 0,
                capture_slow_reads INTEGER NOT NULL DEFAULT 0,
                capture_slow_read_percent REAL NOT NULL DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp_ms INTEGER NOT NULL,
                category TEXT NOT NULL,
                message TEXT NOT NULL,
                UNIQUE(timestamp_ms, category, message)
            );

            CREATE TABLE IF NOT EXISTS reasoning_results (
                request_id TEXT PRIMARY KEY,
                request_timestamp_ms INTEGER NOT NULL,
                timestamp_ms INTEGER NOT NULL,
                trigger TEXT NOT NULL,
                requested_provider TEXT NOT NULL,
                provider TEXT NOT NULL,
                model TEXT NOT NULL,
                risk_level TEXT NOT NULL,
                summary TEXT NOT NULL,
                recommended_action TEXT NOT NULL,
                latency_ms REAL NOT NULL,
                success INTEGER NOT NULL,
                fallback_used INTEGER NOT NULL,
                primary_error TEXT NOT NULL,
                error TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS ingestion_offsets (
                path TEXT PRIMARY KEY,
                offset_bytes INTEGER NOT NULL
            );
            """
        )
        telemetry_columns = {
            row["name"] for row in connection.execute("PRAGMA table_info(telemetry)")
        }
        for name, definition in (
            ("source_fps", "REAL NOT NULL DEFAULT 0"),
            ("capture_delivery_percent", "REAL NOT NULL DEFAULT 0"),
            ("capture_read_avg_ms", "REAL NOT NULL DEFAULT 0"),
            ("capture_read_max_ms", "REAL NOT NULL DEFAULT 0"),
            ("capture_slow_reads", "INTEGER NOT NULL DEFAULT 0"),
            ("capture_slow_read_percent", "REAL NOT NULL DEFAULT 0"),
        ):
            if name not in telemetry_columns:
                connection.execute(f"ALTER TABLE telemetry ADD COLUMN {name} {definition}")
        reasoning_columns = {
            row["name"] for row in connection.execute("PRAGMA table_info(reasoning_results)")
        }
        for name, definition in (
            ("success", "INTEGER NOT NULL DEFAULT 1"),
            ("error", "TEXT NOT NULL DEFAULT ''"),
        ):
            if name not in reasoning_columns:
                connection.execute(
                    f"ALTER TABLE reasoning_results ADD COLUMN {name} {definition}"
                )


def ingest_jsonl(path: Path, insert_sql: str, fields: tuple[str, ...]) -> int:
    if not path.exists():
        return 0

    with connect() as connection:
        offset_row = connection.execute(
            "SELECT offset_bytes FROM ingestion_offsets WHERE path = ?", (str(path),)
        ).fetchone()
        offset = offset_row["offset_bytes"] if offset_row else 0

    file_size = path.stat().st_size
    if offset > file_size:
        offset = 0

    rows: list[tuple[Any, ...]] = []
    with path.open("r", encoding="utf-8") as source:
        source.seek(offset)
        for line in source:
            try:
                record = json.loads(line)
                rows.append(tuple(record[field] for field in fields))
            except (json.JSONDecodeError, KeyError, TypeError):
                continue
        new_offset = source.tell()

    with connect() as connection:
        before = connection.total_changes
        if rows:
            connection.executemany(insert_sql, rows)
        connection.execute(
            """
            INSERT INTO ingestion_offsets (path, offset_bytes) VALUES (?, ?)
            ON CONFLICT(path) DO UPDATE SET offset_bytes = excluded.offset_bytes
            """,
            (str(path), new_offset),
        )
        inserted = connection.total_changes - before - 1

    if new_offset >= JSONL_MAX_BYTES:
        path.write_text("", encoding="utf-8")
        with connect() as connection:
            connection.execute(
                "UPDATE ingestion_offsets SET offset_bytes = 0 WHERE path = ?",
                (str(path),),
            )

    return inserted


def ingest() -> dict[str, int]:
    telemetry_count = ingest_jsonl(
        KPI_DIR / "telemetry.jsonl",
        """
        INSERT OR IGNORE INTO telemetry (
            timestamp_ms, source_fps, capture_fps, capture_delivery_percent,
            detection_fps, preprocess_ms,
            inference_ms, postprocess_ms, persons, rtsp_reconnects,
            last_frame_age_ms, capture_read_avg_ms, capture_read_max_ms,
            capture_slow_reads, capture_slow_read_percent
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            "timestamp_ms",
            "source_fps",
            "capture_fps",
            "capture_delivery_percent",
            "detection_fps",
            "preprocess_ms",
            "inference_ms",
            "postprocess_ms",
            "persons",
            "rtsp_reconnects",
            "last_frame_age_ms",
            "capture_read_avg_ms",
            "capture_read_max_ms",
            "capture_slow_reads",
            "capture_slow_read_percent",
        ),
    )
    event_count = ingest_jsonl(
        KPI_DIR / "events.jsonl",
        "INSERT OR IGNORE INTO events (timestamp_ms, category, message) VALUES (?, ?, ?)",
        ("timestamp_ms", "category", "message"),
    )
    reasoning_count = ingest_jsonl(
        KPI_DIR / "reasoning_results.jsonl",
        """
        INSERT OR IGNORE INTO reasoning_results (
            request_id, request_timestamp_ms, timestamp_ms, trigger,
            requested_provider, provider, model, risk_level, summary,
            recommended_action, latency_ms, fallback_used, primary_error
            , success, error
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            "request_id",
            "request_timestamp_ms",
            "timestamp_ms",
            "trigger",
            "requested_provider",
            "provider",
            "model",
            "risk_level",
            "summary",
            "recommended_action",
            "latency_ms",
            "fallback_used",
            "primary_error",
            "success",
            "error",
        ),
    )
    return {"telemetry": telemetry_count, "events": event_count, "reasoning": reasoning_count}


def cleanup_retention() -> dict[str, int]:
    cutoff_ms = int((time.time() - RETENTION_DAYS * 86400) * 1000)
    with connect() as connection:
        before = connection.total_changes
        connection.execute("DELETE FROM telemetry WHERE timestamp_ms < ?", (cutoff_ms,))
        telemetry_deleted = connection.total_changes - before
        before = connection.total_changes
        connection.execute("DELETE FROM events WHERE timestamp_ms < ?", (cutoff_ms,))
        events_deleted = connection.total_changes - before
        before = connection.total_changes
        connection.execute("DELETE FROM reasoning_results WHERE timestamp_ms < ?", (cutoff_ms,))
        reasoning_deleted = connection.total_changes - before
    return {
        "telemetry": telemetry_deleted,
        "events": events_deleted,
        "reasoning": reasoning_deleted,
    }


def window_cutoff_ms(window_minutes: int) -> int:
    return int((time.time() - window_minutes * 60) * 1000)


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


def event_quality(connection: sqlite3.Connection, cutoff_ms: int) -> dict[str, Any]:
    rows = connection.execute(
        """
        SELECT timestamp_ms, category, message
        FROM events WHERE timestamp_ms >= ?
        ORDER BY timestamp_ms DESC LIMIT 5000
        """,
        (cutoff_ms,),
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


def reasoning_health(connection: sqlite3.Connection, cutoff_ms: int) -> dict[str, Any]:
    aggregate = connection.execute(
        """
        SELECT
            COUNT(*) AS requests,
            SUM(success) AS successes,
            SUM(fallback_used) AS fallbacks,
            AVG(CASE WHEN success THEN latency_ms END) AS latency_ms_avg,
            MAX(CASE WHEN success THEN latency_ms END) AS latency_ms_max
        FROM reasoning_results WHERE timestamp_ms >= ?
        """,
        (cutoff_ms,),
    ).fetchone()
    providers = connection.execute(
        """
        SELECT provider, model, COUNT(*) AS requests,
               SUM(success) AS successes, AVG(latency_ms) AS latency_ms_avg
        FROM reasoning_results WHERE timestamp_ms >= ?
        GROUP BY provider, model ORDER BY requests DESC
        """,
        (cutoff_ms,),
    ).fetchall()

    request_path = KPI_DIR / "reasoning_requests.jsonl"
    offset_path = KPI_DIR / "reasoning_requests.offset"
    pending = 0
    if request_path.exists():
        try:
            offset = int(offset_path.read_text(encoding="utf-8").strip())
        except (OSError, ValueError):
            offset = 0
        try:
            with request_path.open("r", encoding="utf-8") as source:
                source.seek(min(offset, request_path.stat().st_size))
                pending = sum(1 for line in source if line.strip())
        except OSError:
            pending = 0

    result = dict(aggregate)
    requests = result.get("requests", 0) or 0
    successes = result.get("successes", 0) or 0
    result["success_rate_percent"] = round(successes / requests * 100.0, 1) if requests else None
    result["pending"] = pending
    result["providers"] = [dict(row) for row in providers]
    return result


def evaluate_alerts(
    latest: dict[str, Any],
    telemetry_age_sec: float | None,
    device: dict[str, Any],
    quality: dict[str, Any],
    reasoning: dict[str, Any],
) -> list[dict[str, str]]:
    alerts: list[dict[str, str]] = []

    def add(code: str, severity: str, message: str) -> None:
        alerts.append({"code": code, "severity": severity, "message": message})

    if telemetry_age_sec is None or telemetry_age_sec > ALERT_TELEMETRY_AGE_SEC_MAX:
        add("telemetry_stale", "critical", "Vision telemetry is stale.")
    if latest.get("last_frame_age_ms", 0) > ALERT_TELEMETRY_AGE_SEC_MAX * 1000:
        add("stream_stale", "critical", "No fresh camera frame is available.")
    capture_fps = latest.get("capture_fps", ALERT_CAPTURE_FPS_MIN)
    source_fps = latest.get("source_fps", capture_fps)
    delivery_percent = latest.get("capture_delivery_percent", 100)
    if capture_fps < ALERT_CAPTURE_FPS_MIN:
        if source_fps < ALERT_CAPTURE_FPS_MIN:
            add(
                "camera_throughput_low",
                "warning",
                "Camera acquisition is below baseline; inspect the camera or network.",
            )
        else:
            add(
                "capture_delivery_low",
                "warning",
                "Camera acquisition is healthy but delivered capture FPS is low.",
            )
    elif source_fps >= ALERT_CAPTURE_FPS_MIN and delivery_percent < ALERT_CAPTURE_DELIVERY_PERCENT_MIN:
        add(
            "capture_delivery_low",
            "warning",
            "Fresh-frame delivery is persistently below the configured minimum.",
        )
    if latest.get("inference_ms", 0) > ALERT_INFERENCE_MS_MAX:
        add("inference_slow", "warning", "Inference latency is above the configured limit.")
    if device.get("temperature_c") is not None and device["temperature_c"] > ALERT_TEMPERATURE_C_MAX:
        add("temperature_high", "critical", "Pi temperature is above the configured limit.")
    if device.get("throttled") not in (None, "0x0"):
        add("throttling", "critical", "Pi throttling or undervoltage has been detected.")
    if (
        device.get("disk_used_percent") is not None
        and device["disk_used_percent"] > ALERT_DISK_USED_PERCENT_MAX
    ):
        add("disk_high", "critical", "Sentinel storage usage is above the configured limit.")

    tracks = quality.get("unique_tracks", 0)
    short_exits = quality.get("short_zone_exits", 0)
    if tracks >= 5 and short_exits / tracks > ALERT_SHORT_ZONE_EXIT_RATIO_MAX:
        add("zone_jitter", "warning", "Short zone exits indicate excessive boundary jitter.")
    if reasoning.get("pending", 0) > ALERT_REASONING_PENDING_MAX:
        add("reasoning_backlog", "warning", "Reasoning request backlog is above the configured limit.")
    requests = reasoning.get("requests", 0) or 0
    successes = reasoning.get("successes", 0) or 0
    if requests >= 3 and (requests - successes) / requests > ALERT_REASONING_FAILURE_RATIO_MAX:
        add("reasoning_failures", "warning", "Reasoning request failure rate is above the configured limit.")
    return alerts


@app.on_event("startup")
def startup() -> None:
    initialize_database()
    ingest()
    cleanup_retention()


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
def kpi_history(
    limit: Annotated[int, Query(ge=1, le=5000)] = 120,
    window_minutes: Annotated[int, Query(ge=5, le=10080)] = 60,
) -> list[dict[str, Any]]:
    ingest()
    with connect() as connection:
        rows = connection.execute(
            """
            SELECT * FROM telemetry WHERE timestamp_ms >= ?
            ORDER BY timestamp_ms DESC LIMIT ?
            """,
            (window_cutoff_ms(window_minutes), limit),
        ).fetchall()
    return [dict(row) for row in reversed(rows)]


@app.get("/api/events")
def events(
    limit: Annotated[int, Query(ge=1, le=1000)] = 100,
    window_minutes: Annotated[int, Query(ge=5, le=10080)] = 60,
) -> list[dict[str, Any]]:
    ingest()
    with connect() as connection:
        rows = connection.execute(
            """
            SELECT * FROM events WHERE timestamp_ms >= ?
            ORDER BY timestamp_ms DESC, id DESC LIMIT ?
            """,
            (window_cutoff_ms(window_minutes), limit),
        ).fetchall()
    return [dict(row) for row in rows]


@app.get("/api/reasoning")
def reasoning_results(
    limit: Annotated[int, Query(ge=1, le=1000)] = 100,
    window_minutes: Annotated[int, Query(ge=5, le=10080)] = 60,
) -> list[dict[str, Any]]:
    ingest()
    with connect() as connection:
        rows = connection.execute(
            """
            SELECT * FROM reasoning_results WHERE timestamp_ms >= ?
            ORDER BY timestamp_ms DESC LIMIT ?
            """,
            (window_cutoff_ms(window_minutes), limit),
        ).fetchall()
    return [dict(row) for row in rows]


@app.get("/api/summary")
def summary(
    window_minutes: Annotated[int, Query(ge=5, le=10080)] = 60,
) -> dict[str, Any]:
    ingest()
    cutoff_ms = window_cutoff_ms(window_minutes)
    with connect() as connection:
        telemetry = connection.execute(
            """
            SELECT
                COUNT(*) AS samples,
                COUNT(CASE WHEN source_fps > 0 THEN 1 END) AS diagnostic_samples,
                COUNT(CASE WHEN capture_delivery_percent > 0 THEN 1 END)
                    AS normalized_samples,
                AVG(CASE WHEN source_fps > 0 THEN source_fps END) AS source_fps_avg,
                AVG(capture_fps) AS capture_fps_avg,
                AVG(CASE WHEN capture_delivery_percent > 0 THEN capture_delivery_percent END)
                    AS capture_delivery_percent_avg,
                AVG(detection_fps) AS detection_fps_avg,
                AVG(inference_ms) AS inference_ms_avg,
                MAX(rtsp_reconnects) AS rtsp_reconnects,
                MAX(last_frame_age_ms) AS last_frame_age_ms_max,
                AVG(CASE WHEN source_fps > 0 THEN capture_read_avg_ms END)
                    AS capture_read_avg_ms,
                MAX(CASE WHEN source_fps > 0 THEN capture_read_max_ms END)
                    AS capture_read_max_ms,
                SUM(CASE WHEN capture_delivery_percent > 0 THEN capture_slow_reads ELSE 0 END)
                    AS capture_slow_reads,
                AVG(CASE WHEN capture_delivery_percent > 0 THEN capture_slow_read_percent END)
                    AS capture_slow_read_percent_avg
            FROM telemetry WHERE timestamp_ms >= ?
            """,
            (cutoff_ms,),
        ).fetchone()
        categories = connection.execute(
            """
            SELECT category, COUNT(*) AS count FROM events
            WHERE timestamp_ms >= ? GROUP BY category
            """,
            (cutoff_ms,),
        ).fetchall()
        quality = event_quality(connection, cutoff_ms)
        reasoning = reasoning_health(connection, cutoff_ms)

    result = dict(telemetry)
    result["events"] = {row["category"]: row["count"] for row in categories}
    result["event_quality"] = quality
    result["reasoning"] = reasoning
    return result


@app.get("/api/dashboard")
def dashboard_data(
    window_minutes: Annotated[int, Query(ge=5, le=10080)] = 60,
) -> dict[str, Any]:
    ingest()
    cleanup_retention()
    cutoff_ms = window_cutoff_ms(window_minutes)
    with connect() as connection:
        latest = connection.execute(
            "SELECT * FROM telemetry ORDER BY timestamp_ms DESC LIMIT 1"
        ).fetchone()
        history = connection.execute(
            """
            SELECT * FROM telemetry WHERE timestamp_ms >= ?
            ORDER BY timestamp_ms DESC LIMIT 500
            """,
            (cutoff_ms,),
        ).fetchall()
        recent_events = connection.execute(
            """
            SELECT * FROM events WHERE timestamp_ms >= ?
            ORDER BY timestamp_ms DESC, id DESC LIMIT 30
            """,
            (cutoff_ms,),
        ).fetchall()
        quality = event_quality(connection, cutoff_ms)
        reasoning = reasoning_health(connection, cutoff_ms)
        reasoning_results = connection.execute(
            """
            SELECT * FROM reasoning_results WHERE timestamp_ms >= ?
            ORDER BY timestamp_ms DESC LIMIT 20
            """,
            (cutoff_ms,),
        ).fetchall()

    latest_dict = dict(latest) if latest else {}
    latest_timestamp = latest_dict.get("timestamp_ms")
    telemetry_age_sec = (
        round((time.time() * 1000 - latest_timestamp) / 1000, 1)
        if latest_timestamp
        else None
    )

    device = device_health()
    return {
        "latest": latest_dict,
        "telemetry_age_sec": telemetry_age_sec,
        "history": [dict(row) for row in reversed(history)],
        "events": [dict(row) for row in recent_events],
        "event_quality": quality,
        "reasoning_results": [dict(row) for row in reasoning_results],
        "reasoning": reasoning,
        "device": device,
        "alerts": evaluate_alerts(latest_dict, telemetry_age_sec, device, quality, reasoning),
        "window_minutes": window_minutes,
        "retention_days": RETENTION_DAYS,
    }
