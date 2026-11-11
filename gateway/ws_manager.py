import asyncio
import logging
import threading
from dataclasses import dataclass
from typing import Dict, List, Optional

from fastapi import WebSocket

from gateway.mqtt_service import mqtt_service
from gateway.schemas import DeviceTelemetrySchema

logger = logging.getLogger("gateway.ws")


@dataclass
class _ClientSubscription:
    websocket: WebSocket
    device_filter: Optional[str] = None


class TelemetryWebSocketManager:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._clients: Dict[int, _ClientSubscription] = {}
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    def set_event_loop(self, loop: asyncio.AbstractEventLoop) -> None:
        with self._lock:
            self._loop = loop

    @property
    def active_connections_count(self) -> int:
        with self._lock:
            return len(self._clients)

    async def connect(self, websocket: WebSocket, device_filter: Optional[str] = None) -> None:
        await websocket.accept()
        with self._lock:
            self._clients[id(websocket)] = _ClientSubscription(
                websocket=websocket,
                device_filter=device_filter,
            )

        await self._send_initial_snapshot(websocket, device_filter)

    def disconnect(self, websocket: WebSocket) -> None:
        with self._lock:
            self._clients.pop(id(websocket), None)

    async def _send_initial_snapshot(
        self, websocket: WebSocket, device_filter: Optional[str] = None
    ) -> None:
        devices = mqtt_service.get_all_devices()
        for dev in devices:
            if device_filter and dev.deviceName != device_filter:
                continue
            if dev.telemetry is not None:
                try:
                    payload = dev.telemetry.model_dump(mode="json")
                    await websocket.send_json(payload)
                except Exception as exc:
                    logger.debug("Failed to send initial snapshot to WS client: %s", exc)
                    self.disconnect(websocket)
                    return

    def on_mqtt_telemetry(self, telemetry: DeviceTelemetrySchema) -> None:
        with self._lock:
            loop = self._loop
            has_clients = bool(self._clients)

        if not has_clients or loop is None or loop.is_closed():
            return

        payload = telemetry.model_dump(mode="json")
        try:
            asyncio.run_coroutine_threadsafe(
                self.broadcast_telemetry(payload, telemetry.deviceName),
                loop,
            )
        except RuntimeError as exc:
            logger.debug("Could not schedule WS broadcast: %s", exc)

    async def broadcast_telemetry(self, payload: dict, device_name: str) -> None:
        with self._lock:
            subscriptions: List[_ClientSubscription] = list(self._clients.values())

        stale_sockets: List[WebSocket] = []
        for sub in subscriptions:
            if sub.device_filter and sub.device_filter != device_name:
                continue
            try:
                await sub.websocket.send_json(payload)
            except Exception:
                stale_sockets.append(sub.websocket)

        for ws in stale_sockets:
            self.disconnect(ws)


ws_manager = TelemetryWebSocketManager()
