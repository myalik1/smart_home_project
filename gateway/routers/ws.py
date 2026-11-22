from typing import Optional
from fastapi import APIRouter, Query, WebSocket, WebSocketDisconnect

from gateway.ws_manager import ws_manager

router = APIRouter(tags=["WebSocket Telemetry"])


@router.websocket("/ws/telemetry")
@router.websocket("/api/v1/ws/telemetry")
async def websocket_telemetry_endpoint(
    websocket: WebSocket,
    device: Optional[str] = Query(
        default=None,
        description="Опциональное имя устройства (например, Therm1) для фильтрации потока",
    ),
) -> None:

    await ws_manager.connect(websocket, device_filter=device)
    try:
        while True:
            message = await websocket.receive_text()
            if message.strip().lower() == "ping":
                await websocket.send_text("pong")
    except WebSocketDisconnect:
        ws_manager.disconnect(websocket)
    except Exception:
        ws_manager.disconnect(websocket)
