import json
import logging
import threading
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Callable, Dict, List, Optional

import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion

from gateway.config import settings
from gateway.schemas import (
    CommandRequest,
    CommandResponse,
    DeviceStatusResponse,
    DeviceTelemetrySchema,
)

logger = logging.getLogger("gateway.mqtt")


@dataclass
class CachedDeviceState:
    device_name: str
    device_type: str
    topic: str
    last_seen_ms: Optional[int] = None
    telemetry: Optional[DeviceTelemetrySchema] = None


TelemetryCallback = Callable[[DeviceTelemetrySchema], None]


class MqttGatewayService:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._connected = False
        self._devices: Dict[str, CachedDeviceState] = {}
        self._topic_to_name: Dict[str, str] = {}
        self._listeners: List[TelemetryCallback] = []

        self._init_default_devices()

        self._client = mqtt.Client(
            callback_api_version=CallbackAPIVersion.VERSION2,
            client_id=settings.mqtt_client_id,
            clean_session=True,
        )
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message
        self._client.reconnect_delay_set(min_delay=1, max_delay=10)

    def _init_default_devices(self) -> None:
        with self._lock:
            for name, topic in settings.default_device_topics.items():
                dev_type = settings.default_device_types.get(name, "Unknown")
                self._devices[name] = CachedDeviceState(
                    device_name=name,
                    device_type=dev_type,
                    topic=topic,
                )
                self._topic_to_name[topic] = name

    @property
    def is_connected(self) -> bool:
        with self._lock:
            return self._connected

    def start(self) -> None:
        try:
            self._client.connect_async(
                settings.mqtt_broker_host, settings.mqtt_broker_port, keepalive=60
            )
            self._client.loop_start()
            logger.info(
                "MQTT background loop started (target=%s:%s)",
                settings.mqtt_broker_host,
                settings.mqtt_broker_port,
            )
        except Exception as exc:
            logger.warning(
                "MQTT broker unavailable on startup (%s). Running in offline mode with auto-reconnect.",
                exc,
            )

    def stop(self) -> None:
        try:
            self._client.loop_stop()
            if self.is_connected:
                self._client.disconnect()
        except Exception as exc:
            logger.debug("Error during MQTT stop: %s", exc)
        finally:
            with self._lock:
                self._connected = False

    def add_telemetry_listener(self, callback: TelemetryCallback) -> None:
        with self._lock:
            self._listeners.append(callback)

    def _on_connect(self, client: mqtt.Client, userdata, flags, reason_code, properties) -> None:
        if reason_code == 0:
            with self._lock:
                self._connected = True
            wildcard_topic = f"{settings.mqtt_topic_prefix}/#"
            client.subscribe(wildcard_topic, qos=1)
            logger.info("Connected to MQTT broker and subscribed to '%s'", wildcard_topic)
        else:
            logger.warning("MQTT connection failed with reason code: %s", reason_code)

    def _on_disconnect(self, client: mqtt.Client, userdata, flags, reason_code, properties) -> None:
        with self._lock:
            self._connected = False
        logger.warning("Disconnected from MQTT broker (reason=%s)", reason_code)

    def _on_message(self, client: mqtt.Client, userdata, msg: mqtt.MQTTMessage) -> None:
        topic = msg.topic
        if topic.endswith("/set"):
            return

        try:
            payload_str = msg.payload.decode("utf-8")
            raw_data = json.loads(payload_str)
            if not isinstance(raw_data, dict):
                return
        except Exception as exc:
            logger.debug("Ignored malformed JSON on topic '%s': %s", topic, exc)
            return

        self.handle_incoming_telemetry(topic, raw_data)

    def handle_incoming_telemetry(self, topic: str, raw_data: dict) -> Optional[DeviceTelemetrySchema]:
        now_ms = int(datetime.now(timezone.utc).timestamp() * 1000)

        with self._lock:
            dev_name = raw_data.get("deviceName") or self._topic_to_name.get(topic)
            if not dev_name:
                return None

            raw_data["deviceName"] = dev_name
            raw_data.setdefault("timestamp", now_ms)

            try:
                telemetry = DeviceTelemetrySchema.model_validate(raw_data)
            except Exception as exc:
                logger.warning("Validation failed for telemetry from '%s': %s", dev_name, exc)
                return None

            existing = self._devices.get(dev_name)
            dev_type = (
                telemetry.deviceType
                if telemetry.deviceType != "Unknown"
                else (existing.device_type if existing else "Unknown")
            )
            telemetry.deviceType = dev_type

            self._devices[dev_name] = CachedDeviceState(
                device_name=dev_name,
                device_type=dev_type,
                topic=topic,
                last_seen_ms=telemetry.timestamp,
                telemetry=telemetry,
            )
            self._topic_to_name[topic] = dev_name
            listeners_copy = list(self._listeners)

        for listener in listeners_copy:
            try:
                listener(telemetry)
            except Exception as exc:
                logger.error("Error in telemetry listener: %s", exc)

        return telemetry

    def get_all_devices(self) -> List[DeviceStatusResponse]:
        with self._lock:
            result: List[DeviceStatusResponse] = []
            for state in self._devices.values():
                is_online = state.last_seen_ms is not None
                result.append(
                    DeviceStatusResponse(
                        deviceName=state.device_name,
                        deviceType=state.device_type,
                        topic=state.topic,
                        isOnline=is_online,
                        lastSeenMs=state.last_seen_ms,
                        telemetry=state.telemetry,
                    )
                )
            return result

    def get_device(self, device_name: str) -> Optional[DeviceStatusResponse]:
        with self._lock:
            state = self._devices.get(device_name)
            if not state:
                return None
            return DeviceStatusResponse(
                deviceName=state.device_name,
                deviceType=state.device_type,
                topic=state.topic,
                isOnline=(state.last_seen_ms is not None),
                lastSeenMs=state.last_seen_ms,
                telemetry=state.telemetry,
            )

    def publish_command(self, device_name: str, command: CommandRequest) -> CommandResponse:
        with self._lock:
            state = self._devices.get(device_name)
            if not state:
                raise KeyError(f"Device '{device_name}' is not registered")
            cmd_topic = f"{state.topic}/set"

        payload_dict = command.to_mqtt_payload()
        payload_json = json.dumps(payload_dict)

        info = self._client.publish(cmd_topic, payload_json, qos=1)
        status = "sent" if self.is_connected and info.rc == mqtt.MQTT_ERR_SUCCESS else "queued_offline"

        return CommandResponse(
            status=status,
            deviceName=device_name,
            topic=cmd_topic,
            payload=payload_dict,
        )


mqtt_service = MqttGatewayService()
