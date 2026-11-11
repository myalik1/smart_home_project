from gateway.db_service import db_service
from gateway.mqtt_service import MqttGatewayService


def test_db_history_ordering_and_counts(seed_telemetry_db) -> None:
    seed_telemetry_db(device_name="Therm1", timestamp=1000, temperature=19.0)
    seed_telemetry_db(device_name="Therm1", timestamp=2000, temperature=20.5)
    seed_telemetry_db(device_name="Therm1", timestamp=3000, temperature=22.0)
    seed_telemetry_db(device_name="Hygro1", device_type="Hygrometer", timestamp=2500, humidity=55.0)

    assert db_service.get_history_count() == 4
    assert db_service.get_history_count("Therm1") == 3

    # get_device_history должен возвращать записи по убыванию времени (DESC)
    desc_records = db_service.get_device_history("Therm1", limit=2)
    assert [r.timestamp for r in desc_records] == [3000, 2000]

    # get_history_range должен возвращать записи по возрастанию времени (ASC) для графиков
    asc_records = db_service.get_history_range("Therm1", start_timestamp_ms=1000, end_timestamp_ms=2500)
    assert [r.timestamp for r in asc_records] == [1000, 2000]


def test_preload_from_db_keeps_devices_offline_until_live_mqtt(seed_telemetry_db) -> None:
    seed_telemetry_db(device_name="Therm1", timestamp=5000, temperature=23.4)

    latest_map = db_service.get_latest_for_all_devices()
    assert "Therm1" in latest_map

    test_mqtt = MqttGatewayService()
    loaded = test_mqtt.preload_from_db(latest_map)
    assert loaded == 1

    # После предзагрузки из БД данные доступны, но isOnline=False
    therm_before = test_mqtt.get_device("Therm1")
    assert therm_before is not None
    assert therm_before.telemetry is not None
    assert therm_before.isOnline is False
    assert therm_before.lastSeenMs == 5000
    assert therm_before.telemetry.currentTemperature == 23.4

    # После прихода живого MQTT-пакета устройство переходит в isOnline=True
    test_mqtt.handle_incoming_telemetry(
        "home/therm1",
        {"deviceName": "Therm1", "deviceType": "Thermostat", "currentTemperature": 24.1},
    )
    therm_after = test_mqtt.get_device("Therm1")
    assert therm_after is not None
    assert therm_after.telemetry is not None
    assert therm_after.isOnline is True
    assert therm_after.telemetry.currentTemperature == 24.1
