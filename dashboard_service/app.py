import json
import os
import sqlite3
from pathlib import Path
from typing import Any

from fastapi import FastAPI, Query

KPI_DIR = Path(os.environ.get("SENTINEL_KPI_DIR", "/var/lib/sentinel/kpi"))
DATABASE_PATH = Path(
    os.environ.get("SENTINEL_KPI_DATABASE", "/var/lib/sentinel/dashboard/kpi.db")
)

app = FastAPI(title="Sentinel KPI API")


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

    result = dict(telemetry)
    result["events"] = {row["category"]: row["count"] for row in categories}
    return result
