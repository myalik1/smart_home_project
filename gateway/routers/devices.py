from typing import List
from fastapi import APIRouter, HTTPException, status

from gateway.mqtt_service import mqtt_service
from gateway.schemas import CommandRequest, CommandResponse, DeviceStatusResponse

router = APIRouter(prefix="/devices", tags=["Devices"])


@router.get(
    "",
    response_model=List[DeviceStatusResponse],
    summary="Получить список всех устройств и их текущие статусы",
)
async def get_all_devices() -> List[DeviceStatusResponse]:
    return mqtt_service.get_all_devices()


@router.get(
    "/{device_name}",
    response_model=DeviceStatusResponse,
    summary="Получить текущий статус конкретного устройства",
)
async def get_device_by_name(device_name: str) -> DeviceStatusResponse:
    device = mqtt_service.get_device(device_name)
    if device is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Device '{device_name}' not found",
        )
    return device


@router.post(
    "/{device_name}/command",
    response_model=CommandResponse,
    summary="Отправить команду управления устройству через MQTT",
)
async def send_device_command(device_name: str, command: CommandRequest) -> CommandResponse:
    device = mqtt_service.get_device(device_name)
    if device is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Device '{device_name}' not found",
        )

    if not mqtt_service.is_connected:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="MQTT broker is currently unavailable. Command cannot be delivered.",
        )

    try:
        response = mqtt_service.publish_command(device_name, command)
    except KeyError as exc:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=str(exc),
        ) from exc

    if response.status != "sent":
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Failed to publish command to MQTT broker.",
        )

    return response
