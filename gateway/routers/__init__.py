from fastapi import APIRouter

from gateway.routers.devices import router as devices_router
from gateway.routers.history import router as history_router
from gateway.routers.rules import router as rules_router

api_router = APIRouter(prefix="/api/v1")

api_router.include_router(devices_router)
api_router.include_router(history_router)
api_router.include_router(rules_router)
