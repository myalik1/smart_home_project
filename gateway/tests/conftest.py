import json
import sqlite3
from pathlib import Path
from types import SimpleNamespace
from typing import Callable, Generator

import paho.mqtt.client as mqtt
import pytest
from fastapi.testclient import TestClient

from gateway.config import settings
from gateway.db_service import db_service
from gateway.main import app
from gateway.mqtt_service import mqtt_service


@pytest.fixture(autouse=True)
def isolated_gateway_env(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Generator[Path, None, None]:
    """
    Автоматически изолирует каждый тест:
    - Подменяет пути к SQLite БД и rules.json на временную папку tmp_path.
    - Отключает реальные сетевые вызовы Paho-MQTT (start/stop/publish).
    - Сбрасывает кэш устройств mqtt_service перед каждым тестом.
    """
    test_db_path = tmp_path / "test_smart_home.db"
    test_rules_path = tmp_path / "test_rules.json"

    monkeypatch.setattr(settings, "sqlite_db_path", str(test_db_path))
    monkeypatch.setattr(settings, "rules_config_path", str(test_rules_path))

    # Инициализируем чистую схему БД и пустой файл правил
    db_service.init_schema()
    test_rules_path.write_text(json.dumps({"rules": []}, indent=2), encoding="utf-8")

    # Глушим реальные сетевые подключения к Mosquitto во время тестов
    monkeypatch.setattr(mqtt_service, "start", lambda: None)
    monkeypatch.setattr(mqtt_service, "stop", lambda: None)
    monkeypatch.setattr(
        mqtt_service._client,
        "publish",
        lambda topic, payload, qos=1: SimpleNamespace(rc=mqtt.MQTT_ERR_SUCCESS),
    )

    # Сбрасываем состояние кэша устройств к дефолтным 5 приборам
    mqtt_service._devices.clear()
    mqtt_service._topic_to_name.clear()
    mqtt_service._init_default_devices()
    mqtt_service._connected = True

    yield tmp_path


@pytest.fixture()
def client() -> Generator[TestClient, None, None]:
    """Возвращает TestClient с корректно инициализированным lifespan FastAPI."""
    with TestClient(app) as test_client:
        yield test_client


@pytest.fixture()
def seed_telemetry_db() -> Callable[..., None]:
    """Вспомогательная фабрика для вставки тестовых строк в SQLite (имитация записи от C++ ядра)."""
    def _insert(
        device_name: str = "Therm1",
        device_type: str = "Thermostat",
        timestamp: int = 1762605000000,
        is_on: int = 1,
        temperature: float = 21.5,
        target_temperature: float = 24.0,
        humidity: float = 0.0,
        power: float = 0.0,
        brightness: int = 0,
        valve_level: int = 0,
        mode: str = "HEAT",
    ) -> None:
        with sqlite3.connect(settings.db_full_path) as conn:
            conn.execute(
                """
                INSERT INTO telemetry_history (
                    device_name, device_type, serial_number, timestamp, is_on,
                    temperature, target_temperature, humidity, power, brightness,
                    valve_level, target_valve_level, current_valve_level, mode, payload_json
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    device_name,
                    device_type,
                    f"SN-{device_name}",
                    timestamp,
                    is_on,
                    temperature,
                    target_temperature,
                    humidity,
                    power,
                    brightness,
                    valve_level,
                    valve_level,
                    valve_level,
                    mode,
                    "{}",
                ),
            )
            conn.commit()

    return _insert
