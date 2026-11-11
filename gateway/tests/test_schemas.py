import pytest
from pydantic import ValidationError

from gateway.schemas import (
    CommandRequest,
    CommandTypeEnum,
    DeviceTelemetrySchema,
    RuleActionSchema,
    RuleItemSchema,
    ThermostatModeEnum,
)


def test_command_on_off_valid() -> None:
    cmd = CommandRequest(command=CommandTypeEnum.ON)
    assert cmd.to_mqtt_payload() == {"command": "ON"}


def test_command_set_mode_requires_mode_field() -> None:
    with pytest.raises(ValidationError):
        CommandRequest(command=CommandTypeEnum.SET_MODE)

    cmd = CommandRequest(command=CommandTypeEnum.SET_MODE, mode=ThermostatModeEnum.HEAT)
    assert cmd.to_mqtt_payload() == {"command": "SET_MODE", "mode": "HEAT"}


def test_command_numeric_requires_value_and_passes_out_of_range_for_cpp_clamp() -> None:
    with pytest.raises(ValidationError):
        CommandRequest(command=CommandTypeEnum.SET_BRIGHTNESS)

    # Дробное 80.0 для яркости приводится к int 80
    cmd_lamp = CommandRequest(command=CommandTypeEnum.SET_BRIGHTNESS, value=80.0)
    assert cmd_lamp.to_mqtt_payload() == {"command": "SET_BRIGHTNESS", "value": 80}

    # Значения вне диапазона (150.5) пропускаются для std::clamp в C++
    cmd_rad = CommandRequest(command=CommandTypeEnum.SET_VALVE_LEVEL, value=150.5)
    assert cmd_rad.to_mqtt_payload() == {"command": "SET_VALVE_LEVEL", "value": 150.5}


def test_telemetry_normalization_and_sqlite_conversion() -> None:
    # Проверка маппинга humidity -> currentHumidity и синхронизации valveLevel
    dto = DeviceTelemetrySchema.model_validate(
        {
            "deviceName": "Rad1",
            "deviceType": "Radiator",
            "timestamp": 1762605000123,
            "humidity": 48.5,
            "valveLevel": 65,
        }
    )
    assert dto.currentHumidity == 48.5
    assert dto.targetValveLevel == 65
    assert dto.currentValveLevel == 65
    assert dto.timestampIso is not None

    # Проверка фабричного метода from_sqlite_row
    sqlite_row = {
        "device_name": "Plug1",
        "device_type": "Plug",
        "serial_number": "P-100",
        "timestamp": 1762605005000,
        "is_on": 1,
        "power": 1250.5,
    }
    from_db = DeviceTelemetrySchema.from_sqlite_row(sqlite_row)
    assert from_db.deviceName == "Plug1"
    assert from_db.isOn is True
    assert from_db.currentPower == 1250.5


def test_rule_action_requires_device_or_topic() -> None:
    with pytest.raises(ValidationError):
        RuleActionSchema(command={"command": "ON"})

    rule = RuleItemSchema(
        id="rule_1",
        type="point",
        time="08:00:00",
        action=RuleActionSchema(device="Lamp1", command={"command": "ON"}),
    )
    assert rule.id == "rule_1"
