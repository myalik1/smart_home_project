import json
from contextlib import asynccontextmanager
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from gateway.config import settings
from gateway.mqtt_service import mqtt_service


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings.data_dir.mkdir(parents=True, exist_ok=True)

    if not settings.rules_full_path.exists():
        settings.rules_full_path.write_text(
            json.dumps({"rules": []}, indent=2), encoding="utf-8"
        )

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
        "sqlite_db_path": str(settings.db_full_path),
        "sqlite_db_exists": settings.db_full_path.exists(),
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
