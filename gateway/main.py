import asyncio
import json
from contextlib import asynccontextmanager
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from gateway.config import settings
from gateway.mqtt_service import mqtt_service
from gateway.db_service import db_service
from gateway.routers import api_router
from gateway.routers.ws import router as ws_router
from gateway.ws_manager import ws_manager


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings.data_dir.mkdir(parents=True, exist_ok=True)

    if not settings.rules_full_path.exists():
        settings.rules_full_path.write_text(
            json.dumps({"rules": []}, indent=2), encoding="utf-8"
        )

    db_service.init_schema()
    latest_db_states = db_service.get_latest_for_all_devices()
    if latest_db_states:
        mqtt_service.preload_from_db(latest_db_states)

    ws_manager.set_event_loop(asyncio.get_running_loop())
    mqtt_service.add_telemetry_listener(ws_manager.on_mqtt_telemetry)

    mqtt_service.start()

    yield

    mqtt_service.stop()


app = FastAPI(
    title=settings.app_name,
    version=settings.app_version,
    description="REST & WebSocket API Gateway for C++ Smart Home System",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(api_router)
app.include_router(ws_router)

@app.get("/health", tags=["System"])
@app.get("/api/v1/health", tags=["System"])
async def health_check():
    devices = mqtt_service.get_all_devices()
    online_devices = [d.deviceName for d in devices if d.isOnline]

    return {
        "status": "ok",
        "service": settings.app_name,
        "version": settings.app_version,
        "mqtt_broker": f"{settings.mqtt_broker_host}:{settings.mqtt_broker_port}",
        "mqtt_connected": mqtt_service.is_connected,
        "sqlite_db_path": str(settings.db_full_path),
        "sqlite_db_exists": settings.db_full_path.exists(),
        "telemetry_records_count": db_service.get_history_count(),
        "rules_config_path": str(settings.rules_full_path),
        "registered_devices_count": len(devices),
        "online_devices": online_devices,
    }


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "gateway.main:app",
        host=settings.host,
        port=settings.port,
        reload=settings.debug,
    )
