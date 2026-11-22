import threading
from fastapi.testclient import TestClient

from gateway.mqtt_service import mqtt_service


def test_websocket_initial_snapshot_live_broadcast_and_filter(client: TestClient) -> None:
    # 1. Предварительно записываем в кэш два разных устройства
    mqtt_service.handle_incoming_telemetry(
        "home/lamp1",
        {"deviceName": "Lamp1", "deviceType": "Lamp", "isOn": True, "brightness": 45},
    )
    mqtt_service.handle_incoming_telemetry(
        "home/therm1",
        {"deviceName": "Therm1", "deviceType": "Thermostat", "currentTemperature": 22.5},
    )

    # 2. Подключаемся с фильтром ?device=Lamp1
    with client.websocket_connect("/ws/telemetry?device=Lamp1") as ws:
        # Должен прийти начальный снимок только для Lamp1 (плоский DeviceTelemetrySchema)
        initial = ws.receive_json()
        assert initial["deviceName"] == "Lamp1"
        assert initial["brightness"] == 45

        # Имитируем приход телеметрии от Therm1 (должна быть отфильтрована)
        # и затем от Lamp1 (должна прийти в сокет) из фонового потока
        def _background_mqtt_updates() -> None:
            mqtt_service.handle_incoming_telemetry(
                "home/therm1",
                {"deviceName": "Therm1", "deviceType": "Thermostat", "currentTemperature": 25.0},
            )
            mqtt_service.handle_incoming_telemetry(
                "home/lamp1",
                {"deviceName": "Lamp1", "deviceType": "Lamp", "isOn": True, "brightness": 90},
            )

        worker = threading.Thread(target=_background_mqtt_updates)
        worker.start()
        worker.join()

        live_msg = ws.receive_json()
        assert live_msg["deviceName"] == "Lamp1"
        assert live_msg["brightness"] == 90

        # Проверка ping -> pong
        ws.send_text("ping")
        assert ws.receive_text() == "pong"
