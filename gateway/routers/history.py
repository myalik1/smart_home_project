from typing import List, Optional
from fastapi import APIRouter, HTTPException, Query, status

from gateway.db_service import db_service
from gateway.mqtt_service import mqtt_service
from gateway.schemas import DeviceTelemetrySchema

router = APIRouter(tags=["Telemetry History"])


@router.get(
    "/devices/{device_name}/history",
    response_model=List[DeviceTelemetrySchema],
    summary="Получить историю телеметрии конкретного устройства",
)
async def get_device_telemetry_history(
    device_name: str,
    limit: int = Query(default=100, ge=1, le=5000, description="Максимальное число записей"),
    start_ts: Optional[int] = Query(default=None, ge=0, description="Начало интервала (Unix ms)"),
    end_ts: Optional[int] = Query(default=None, ge=0, description="Конец интервала (Unix ms)"),
) -> List[DeviceTelemetrySchema]:

    known_in_cache = mqtt_service.get_device(device_name) is not None
    known_in_db = device_name in db_service.get_registered_device_names()
    if not known_in_cache and not known_in_db:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Device '{device_name}' not found",
        )

    if (start_ts is None) != (end_ts is None):
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Both 'start_ts' and 'end_ts' must be provided together for a range query.",
        )

    if start_ts is not None and end_ts is not None:
        if start_ts > end_ts:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="'start_ts' must be less than or equal to 'end_ts'.",
            )
        return db_service.get_history_range(
            device_name=device_name,
            start_timestamp_ms=start_ts,
            end_timestamp_ms=end_ts,
            limit=limit,
        )

    return db_service.get_device_history(device_name=device_name, limit=limit)


@router.get(
    "/history",
    response_model=List[DeviceTelemetrySchema],
    summary="Получить общую ленту последних записей телеметрии по всем устройствам",
)
async def get_all_telemetry_history(
    limit: int = Query(default=100, ge=1, le=5000, description="Максимальное число записей"),
) -> List[DeviceTelemetrySchema]:
    return db_service.get_all_history(limit=limit)
