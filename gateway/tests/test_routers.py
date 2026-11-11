from fastapi.testclient import TestClient

from gateway.mqtt_service import mqtt_service


def test_health_endpoint(client: TestClient) -> None:
    resp = client.get("/api/v1/health")
    assert resp.status_code == 200
    data = resp.json()
    assert data["status"] == "ok"
    assert data["registered_devices_count"] == 5
    assert "active_ws_clients" in data


def test_devices_and_command_endpoints(client: TestClient) -> None:
    # 1. Список всех 5 дефолтных устройств
    resp_all = client.get("/api/v1/devices")
    assert resp_all.status_code == 200
    assert len(resp_all.json()) == 5

    # 2. Неизвестное устройство -> 404
    assert client.get("/api/v1/devices/UnknownDev").status_code == 404

    # 3. Успешная отправка команды при активном брокере -> 200 OK
    mqtt_service._connected = True
    cmd_ok = client.post(
        "/api/v1/devices/Lamp1/command",
        json={"command": "SET_BRIGHTNESS", "value": 75},
    )
    assert cmd_ok.status_code == 200
    assert cmd_ok.json()["status"] == "sent"
    assert cmd_ok.json()["topic"] == "home/lamp1/set"
    assert cmd_ok.json()["payload"] == {"command": "SET_BRIGHTNESS", "value": 75}

    # 4. Отправка команды при отключенном брокере -> 503 Service Unavailable
    mqtt_service._connected = False
    cmd_offline = client.post("/api/v1/devices/Lamp1/command", json={"command": "ON"})
    assert cmd_offline.status_code == 503


def test_history_router_validation_and_queries(client: TestClient, seed_telemetry_db) -> None:
    seed_telemetry_db(device_name="Therm1", timestamp=1000, temperature=20.0)
    seed_telemetry_db(device_name="Therm1", timestamp=2000, temperature=21.0)

    # Обычный запрос истории (DESC)
    r_desc = client.get("/api/v1/devices/Therm1/history?limit=10")
    assert r_desc.status_code == 200
    assert [item["timestamp"] for item in r_desc.json()] == [2000, 1000]

    # Запрос интервала (ASC)
    r_asc = client.get("/api/v1/devices/Therm1/history?start_ts=1000&end_ts=2000")
    assert r_asc.status_code == 200
    assert [item["timestamp"] for item in r_asc.json()] == [1000, 2000]

    # Ошибка 400, если передан только один из параметров интервала или start_ts > end_ts
    assert client.get("/api/v1/devices/Therm1/history?start_ts=1000").status_code == 400
    assert client.get("/api/v1/devices/Therm1/history?start_ts=3000&end_ts=1000").status_code == 400


def test_rules_crud_endpoints(client: TestClient) -> None:
    # Изначально список правил пуст
    r_get = client.get("/api/v1/rules")
    assert r_get.status_code == 200
    assert r_get.json()["rules"] == []

    rule_item = {
        "id": "night_off",
        "type": "point",
        "enabled": True,
        "time": "23:00:00",
        "repeatDaily": True,
        "action": {"device": "Lamp1", "command": {"command": "OFF"}},
    }

    # Добавление правила -> 201 Created
    r_add = client.post("/api/v1/rules/items", json=rule_item)
    assert r_add.status_code == 201
    assert r_add.json()["id"] == "night_off"

    # Повторное добавление с тем же ID -> 409 Conflict
    assert client.post("/api/v1/rules/items", json=rule_item).status_code == 409

    # Полная перезапись через PUT с дубликатами ID -> 400 Bad Request
    r_dup = client.put("/api/v1/rules", json={"rules": [rule_item, rule_item]})
    assert r_dup.status_code == 400

    # Удаление правила -> 200 OK
    r_del = client.delete("/api/v1/rules/items/night_off")
    assert r_del.status_code == 200
    assert r_del.json()["rules"] == []

    # Повторное удаление несуществующего правила -> 404 Not Found
    assert client.delete("/api/v1/rules/items/night_off").status_code == 404
