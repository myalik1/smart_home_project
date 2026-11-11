from datetime import datetime, timezone
from enum import Enum
from typing import Any, Dict, List, Optional, Union
from pydantic import BaseModel, ConfigDict, Field, model_validator



class DeviceTypeEnum(str, Enum):
    LAMP = "Lamp"
    PLUG = "Plug"
    RADIATOR = "Radiator"
    THERMOSTAT = "Thermostat"
    HYGROMETER = "Hygrometer"


class CommandTypeEnum(str, Enum):
    ON = "ON"
    OFF = "OFF"
    SET_BRIGHTNESS = "SET_BRIGHTNESS"
    SET_TEMPERATURE = "SET_TEMPERATURE"
    SET_MODE = "SET_MODE"
    UPDATE_CURRENT_TEMP = "UPDATE_CURRENT_TEMP"
    SET_VALVE_LEVEL = "SET_VALVE_LEVEL"
    UPDATE_POWER = "UPDATE_POWER"
    UPDATE_HUMIDITY = "UPDATE_HUMIDITY"


class ThermostatModeEnum(str, Enum):
    OFF = "OFF"
    HEAT = "HEAT"
    COOL = "COOL"


class RuleTypeEnum(str, Enum):
    POINT = "point"
    TIME_OF_DAY = "time_of_day"
    POINT_ABSOLUTE = "point_absolute"
    TIMESTAMP = "timestamp"
    TIME_WINDOW = "time_window"
    INTERVAL_WINDOW = "interval_window"
    PERIODIC = "periodic"

class CommandRequest(BaseModel):
    command: CommandTypeEnum = Field(..., description="Тип команды управления")
    value: Optional[float] = Field(default=None, description="Числовое значение параметра")
    mode: Optional[ThermostatModeEnum] = Field(default=None, description="Режим работы термостата")

    @model_validator(mode="after")
    def validate_command_parameters(self) -> "CommandRequest":
        if self.command in (CommandTypeEnum.ON, CommandTypeEnum.OFF):
            return self

        if self.command == CommandTypeEnum.SET_MODE:
            if self.mode is None:
                raise ValueError("Field 'mode' ('OFF', 'HEAT', 'COOL') is required for command 'SET_MODE'")
            return self

        if self.value is None:
            raise ValueError(f"Numeric field 'value' is required for command '{self.command.value}'")

        return self

    def to_mqtt_payload(self) -> Dict[str, Any]:
        payload: Dict[str, Any] = {"command": self.command.value}
        if self.command == CommandTypeEnum.SET_MODE and self.mode is not None:
            payload["mode"] = self.mode.value
        elif self.value is not None:
            if self.command == CommandTypeEnum.SET_BRIGHTNESS and self.value.is_integer():
                payload["value"] = int(self.value)
            else:
                payload["value"] = self.value
        return payload


class CommandResponse(BaseModel):
    status: str = "sent"
    deviceName: str
    topic: str
    payload: Dict[str, Any]

class DeviceTelemetrySchema(BaseModel):
    model_config = ConfigDict(populate_by_name=True, extra="ignore")

    deviceName: str = Field(..., description="Уникальное имя устройства (например, Lamp1)")
    deviceType: str = Field(default="Unknown", description="Тип устройства")
    serialNumber: str = Field(default="", description="Серийный номер")
    timestamp: int = Field(
        default_factory=lambda: int(datetime.now(timezone.utc).timestamp() * 1000),
        description="Временная метка Unix в миллисекундах (int64 для QDateTime::fromMSecsSinceEpoch)",
    )
    timestampIso: Optional[str] = Field(
        default=None,
        description="Человекочитаемая дата в ISO-8601 (UTC) для удобства отображения и отладки",
    )

    isOn: bool = False
    currentTemperature: float = 0.0
    targetTemperature: float = 0.0
    currentHumidity: float = 0.0
    currentPower: float = 0.0
    brightness: int = 0
    valveLevel: int = 0
    targetValveLevel: int = 0
    currentValveLevel: int = 0
    mode: str = "OFF"

    @model_validator(mode="before")
    @classmethod
    def normalize_incoming_data(cls, data: Any) -> Any:
        if not isinstance(data, dict):
            return data

        normalized = dict(data)

        if "currentHumidity" not in normalized and "humidity" in normalized:
            normalized["currentHumidity"] = normalized["humidity"]

        if "valveLevel" in normalized and normalized.get("valveLevel", 0) != 0:
             normalized.setdefault("targetValveLevel", normalized["valveLevel"])
             normalized.setdefault("currentValveLevel", normalized["valveLevel"])
        elif "currentValveLevel" in normalized and normalized.get("valveLevel", 0) == 0:
            normalized["valveLevel"] = normalized["currentValveLevel"]
        elif "targetValveLevel" in normalized and normalized.get("valveLevel", 0) == 0:
            normalized["valveLevel"] = normalized["targetValveLevel"]

        return normalized

    @model_validator(mode="after")
    def populate_iso_timestamp(self) -> "DeviceTelemetrySchema":
        if self.timestampIso is None and self.timestamp > 0:
            dt = datetime.fromtimestamp(self.timestamp / 1000.0, tz=timezone.utc)
            self.timestampIso = dt.isoformat()
        return self

    @classmethod
    def from_sqlite_row(cls, row: Dict[str, Any]) -> "DeviceTelemetrySchema":
        """Фабричный метод для конвертации snake_case строки из SQLite telemetry_history в camelCase DTO."""
        is_on_raw = row.get("is_on", 0)
        is_on = str(is_on_raw).lower() in ("1", "true")

        return cls(
            deviceName=row.get("device_name", ""),
            deviceType=row.get("device_type", "Unknown"),
            serialNumber=row.get("serial_number") or "",
            timestamp=int(row.get("timestamp", 0)),
            isOn=is_on,
            currentTemperature=float(row.get("temperature", 0.0) or 0.0),
            targetTemperature=float(row.get("target_temperature", 0.0) or 0.0),
            currentHumidity=float(row.get("humidity", 0.0) or 0.0),
            currentPower=float(row.get("power", 0.0) or 0.0),
            brightness=int(row.get("brightness", 0) or 0),
            valveLevel=int(row.get("valve_level", 0) or 0),
            targetValveLevel=int(row.get("target_valve_level", 0) or 0),
            currentValveLevel=int(row.get("current_valve_level", 0) or 0),
            mode=row.get("mode") or "OFF",
        )


class DeviceStatusResponse(BaseModel):
    deviceName: str
    deviceType: str
    topic: str
    isOnline: bool = False
    lastSeenMs: Optional[int] = None
    telemetry: Optional[DeviceTelemetrySchema] = None

class RuleActionSchema(BaseModel):
    device: Optional[str] = Field(default=None, description="Имя целевого устройства (например, Lamp1)")
    topic: Optional[str] = Field(default=None, description="Прямой MQTT-топик, если устройство не указано")
    command: Union[Dict[str, Any], str] = Field(..., description="JSON-объект или строка команды")

    @model_validator(mode="after")
    def check_target_exists(self) -> "RuleActionSchema":
        if not self.device and not self.topic:
            raise ValueError("Either 'device' or 'topic' must be specified in RuleAction")
        return self


class RuleItemSchema(BaseModel):
    id: str = Field(..., min_length=1, description="Уникальный идентификатор правила/таймера")
    type: RuleTypeEnum = Field(default=RuleTypeEnum.POINT)
    enabled: bool = True

    time: Optional[str] = Field(default=None, pattern=r"^\d{1,2}:\d{2}(:\d{2})?$")
    repeatDaily: Optional[bool] = None

    timestamp: Optional[int] = None

    startTime: Optional[str] = Field(default=None, pattern=r"^\d{1,2}:\d{2}(:\d{2})?$")
    endTime: Optional[str] = Field(default=None, pattern=r"^\d{1,2}:\d{2}(:\d{2})?$")
    maintainState: Optional[bool] = None
    maintainIntervalSeconds: Optional[int] = Field(default=None, gt=0)
    startAction: Optional[RuleActionSchema] = None
    endAction: Optional[RuleActionSchema] = None

    intervalMs: Optional[int] = Field(default=None, gt=0)
    intervalSeconds: Optional[int] = Field(default=None, gt=0)
    maxTicks: Optional[int] = Field(default=None, ge=0)

    action: Optional[RuleActionSchema] = None


class RulesConfigSchema(BaseModel):
    rules: List[RuleItemSchema] = Field(default_factory=list)
