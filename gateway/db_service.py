import logging
import sqlite3
from pathlib import Path
from typing import Dict, List, Optional

from gateway.config import settings
from gateway.schemas import DeviceTelemetrySchema

logger = logging.getLogger("gateway.db")


class TelemetryDatabaseService:
    def __init__(self, db_path: Optional[Path] = None) -> None:
        self._db_path = db_path

    @property
    def db_path(self) -> Path:
        return self._db_path if self._db_path is not None else settings.db_full_path

    def _get_connection(self) -> sqlite3.Connection:
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        conn = sqlite3.connect(str(self.db_path), timeout=5.0, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA busy_timeout = 5000;")
        conn.execute("PRAGMA journal_mode = WAL;")
        return conn

    def init_schema(self) -> bool:
        create_table_sql = """
        CREATE TABLE IF NOT EXISTS telemetry_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_name TEXT NOT NULL,
            device_type TEXT NOT NULL,
            serial_number TEXT,
            timestamp INTEGER NOT NULL,
            is_on INTEGER DEFAULT 0,
            temperature REAL DEFAULT 0.0,
            target_temperature REAL DEFAULT 0.0,
            humidity REAL DEFAULT 0.0,
            power REAL DEFAULT 0.0,
            brightness INTEGER DEFAULT 0,
            valve_level INTEGER DEFAULT 0,
            target_valve_level INTEGER DEFAULT 0,
            current_valve_level INTEGER DEFAULT 0,
            mode TEXT DEFAULT 'OFF',
            payload_json TEXT NOT NULL
        );
        """
        try:
            with self._get_connection() as conn:
                conn.execute(create_table_sql)
                conn.execute(
                    "CREATE INDEX IF NOT EXISTS idx_telemetry_dev ON telemetry_history(device_name);"
                )
                conn.execute(
                    "CREATE INDEX IF NOT EXISTS idx_telemetry_time ON telemetry_history(timestamp);"
                )
                conn.execute(
                    "CREATE INDEX IF NOT EXISTS idx_telemetry_dev_time ON telemetry_history(device_name, timestamp);"
                )
                conn.commit()
            return True
        except sqlite3.Error as exc:
            logger.error("Failed to initialize SQLite schema at '%s': %s", self.db_path, exc)
            return False

    def get_device_history(self, device_name: str, limit: int = 100) -> List[DeviceTelemetrySchema]:
        if limit <= 0:
            return []

        sql = """
        SELECT * FROM telemetry_history
        WHERE device_name = ?
        ORDER BY timestamp DESC
        LIMIT ?;
        """
        try:
            with self._get_connection() as conn:
                cursor = conn.execute(sql, (device_name, limit))
                rows = cursor.fetchall()
            return [DeviceTelemetrySchema.from_sqlite_row(dict(row)) for row in rows]
        except sqlite3.Error as exc:
            logger.error("Error querying history for device '%s': %s", device_name, exc)
            return []

    def get_all_history(self, limit: int = 100) -> List[DeviceTelemetrySchema]:
        if limit <= 0:
            return []

        sql = """
        SELECT * FROM telemetry_history
        ORDER BY timestamp DESC
        LIMIT ?;
        """
        try:
            with self._get_connection() as conn:
                cursor = conn.execute(sql, (limit,))
                rows = cursor.fetchall()
            return [DeviceTelemetrySchema.from_sqlite_row(dict(row)) for row in rows]
        except sqlite3.Error as exc:
            logger.error("Error querying all history: %s", exc)
            return []

    def get_history_range(
        self,
        device_name: str,
        start_timestamp_ms: int,
        end_timestamp_ms: int,
        limit: Optional[int] = None,
    ) -> List[DeviceTelemetrySchema]:

        sql = """
        SELECT * FROM telemetry_history
        WHERE device_name = ? AND timestamp >= ? AND timestamp <= ?
        ORDER BY timestamp ASC
        """
        params: list = [device_name, start_timestamp_ms, end_timestamp_ms]

        if limit is not None and limit > 0:
            sql += " LIMIT ?;"
            params.append(limit)
        else:
            sql += ";"

        try:
            with self._get_connection() as conn:
                cursor = conn.execute(sql, tuple(params))
                rows = cursor.fetchall()
            return [DeviceTelemetrySchema.from_sqlite_row(dict(row)) for row in rows]
        except sqlite3.Error as exc:
            logger.error(
                "Error querying history range for device '%s' [%s..%s]: %s",
                device_name,
                start_timestamp_ms,
                end_timestamp_ms,
                exc,
            )
            return []

    def get_latest_recorded(self, device_name: str) -> Optional[DeviceTelemetrySchema]:
        records = self.get_device_history(device_name, limit=1)
        return records[0] if records else None

    def get_latest_for_all_devices(self) -> Dict[str, DeviceTelemetrySchema]:
        sql = """
        SELECT t.*
        FROM telemetry_history t
        INNER JOIN (
            SELECT device_name, MAX(id) AS max_id
            FROM telemetry_history
            GROUP BY device_name
        ) latest ON t.id = latest.max_id;
        """
        result: Dict[str, DeviceTelemetrySchema] = {}
        try:
            with self._get_connection() as conn:
                cursor = conn.execute(sql)
                rows = cursor.fetchall()
            for row in rows:
                dto = DeviceTelemetrySchema.from_sqlite_row(dict(row))
                if dto.deviceName:
                    result[dto.deviceName] = dto
        except sqlite3.Error as exc:
            logger.error("Error loading latest states per device from SQLite: %s", exc)
        return result

    def get_registered_device_names(self) -> List[str]:
        sql = "SELECT DISTINCT device_name FROM telemetry_history ORDER BY device_name ASC;"
        try:
            with self._get_connection() as conn:
                cursor = conn.execute(sql)
                return [row["device_name"] for row in cursor.fetchall() if row["device_name"]]
        except sqlite3.Error as exc:
            logger.error("Error querying distinct device names: %s", exc)
            return []

    def get_history_count(self, device_name: Optional[str] = None) -> int:
        try:
            with self._get_connection() as conn:
                if device_name:
                    cursor = conn.execute(
                        "SELECT COUNT(*) AS cnt FROM telemetry_history WHERE device_name = ?;",
                        (device_name,),
                    )
                else:
                    cursor = conn.execute("SELECT COUNT(*) AS cnt FROM telemetry_history;")
                row = cursor.fetchone()
                return int(row["cnt"]) if row else 0
        except sqlite3.Error as exc:
            logger.error("Error counting telemetry records: %s", exc)
            return 0


db_service = TelemetryDatabaseService()
