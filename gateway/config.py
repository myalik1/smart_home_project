from pathlib import Path
from typing import Dict
from pydantic_settings import BaseSettings, SettingsConfigDict

PROJECT_ROOT = Path(__file__).resolve().parent.parent


class Settings(BaseSettings):
    app_name: str = "Smart Home API Gateway"
    app_version: str = "1.0.0"
    debug: bool = True

    # HTTP server
    host: str = "127.0.0.1"
    port: int = 8000

    # Mosquitto settings
    mqtt_broker_host: str = "localhost"
    mqtt_broker_port: int = 1883
    mqtt_client_id: str = "smart_home_api_gateway"
    mqtt_topic_prefix: str = "home"

    # Path to data/
    sqlite_db_path: str = "data/smart_home.db"
    rules_config_path: str = "data/rules.json"

    default_device_topics: Dict[str, str] = {
        "Lamp1": "home/lamp1",
        "Plug1": "home/plug1",
        "Rad1": "home/radiator1",
        "Therm1": "home/therm1",
        "Hygro1": "home/hygro1",
    }

    model_config = SettingsConfigDict(
        env_file=str(PROJECT_ROOT / "gateway" / ".env"),
        env_file_encoding="utf-8",
        extra="ignore",
    )

    @property
    def db_full_path(self) -> Path:
        path = Path(self.sqlite_db_path)
        return path if path.is_absolute() else PROJECT_ROOT / path

    @property
    def rules_full_path(self) -> Path:
        path = Path(self.rules_config_path)
        return path if path.is_absolute() else PROJECT_ROOT / path

    @property
    def data_dir(self) -> Path:
        return self.db_full_path.parent


settings = Settings()
