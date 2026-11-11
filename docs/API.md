# Документация Smart Home API Gateway (v1.0.0)

Сетевой шлюз на базе FastAPI обеспечивает двустороннюю связь между клиентским приложением (C++/Qt GUI), брокером сообщений MQTT (Mosquitto), базой данных исторической телеметрии SQLite (`data/smart_home.db`) и конфигурацией правил автоматизации (`data/rules.json`).

## 1. Базовые параметры подключения

* **Базовый URL (REST API):** `[http://127.0.0.1:8001](http://127.0.0.1:8001)` (или порт, заданный в `gateway/config.py` / `.env`)
* **Префикс API:** `/api/v1`
* **Базовый URL (WebSocket):** `ws://127.0.0.1:8001/ws/telemetry` (алиас: `ws://127.0.0.1:8001/api/v1/ws/telemetry`)
* **Формат данных:** `application/json` (кодировка `UTF-8`)
* **Стиль именования полей в DTO:** `camelCase` (полностью совместим с JSON-пакетами C++ микро-приложений)
* **Интерактивная документация (Swagger UI):** `[http://127.0.0.1:8001/docs](http://127.0.0.1:8001/docs)`
* **Альтернативная документация (ReDoc):** `[http://127.0.0.1:8001/redoc](http://127.0.0.1:8001/redoc)`

### Предустановленные устройства в системе

При старте шлюз автоматически регистрирует 5 штатных устройств проекта:

| Имя (`deviceName`) | Тип (`deviceType`) | Топик телеметрии (`topic`) | Топик команд |
| --- | --- | --- | --- |
| `Lamp1` | `Lamp` | `home/lamp1` | `home/lamp1/set` |
| `Plug1` | `Plug` | `home/plug1` | `home/plug1/set` |
| `Rad1` | `Radiator` | `home/radiator1` | `home/radiator1/set` |
| `Therm1` | `Thermostat` | `home/therm1` | `home/therm1/set` |
| `Hygro1` | `Hygrometer` | `home/hygro1` | `home/hygro1/set` |

---

## 2. Базовые модели данных (DTO)

### 2.1. Объект телеметрии (`DeviceTelemetrySchema`)

Единый универсальный объект показателей устройства. Возвращается внутри карточек устройств, в ответах истории из SQLite и в потоке WebSocket.

```json
{
  "deviceName": "Therm1",
  "deviceType": "Thermostat",
  "serialNumber": "SN-Therm1",
  "timestamp": 1762605000123,
  "timestampIso": "2025-11-08T12:30:00.123000+00:00",
  "isOn": true,
  "currentTemperature": 21.5,
  "targetTemperature": 24.0,
  "currentHumidity": 0.0,
  "currentPower": 0.0,
  "brightness": 0,
  "valveLevel": 0,
  "targetValveLevel": 0,
  "currentValveLevel": 0,
  "mode": "HEAT"
}

```

**Описание полей `DeviceTelemetrySchema`:**

* `deviceName` (`string`) — уникальное системное имя прибора (`Lamp1`, `Therm1` и т.д.).
* `deviceType` (`string`) — тип устройства (`Lamp`, `Plug`, `Radiator`, `Thermostat`, `Hygrometer`, `Unknown`).
* `serialNumber` (`string`, *nullable*) — серийный номер прибора.
* `timestamp` (`int64`) — время фиксации состояния в формате Unix Epoch **в миллисекундах (UTC)**.
* `timestampIso` (`string`, *nullable*) — автоматически вычисляемая строка даты/времени в формате ISO-8601 UTC (удобно для отображения в UI).
* `isOn` (`bool`) — включено ли питание устройства (`true` / `false`).
* `currentTemperature` (`float`) — текущая температура в °C (актуально для `Thermostat`, `Radiator`, `Hygrometer`).
* `targetTemperature` (`float`) — целевая температура в °C (для `Thermostat`).
* `currentHumidity` (`float`) — текущая влажность воздуха в `%` (для `Hygrometer`).
* `currentPower` (`float`) — текущая потребляемая мощность в Ваттах (для `Plug`).
* `brightness` (`int`) — уровень яркости от `0` до `100` (для `Lamp`).
* `valveLevel`, `targetValveLevel`, `currentValveLevel` (`int`) — степень открытия клапана радиатора от `0` до `100` (для `Radiator`, все три поля синхронизированы).
* `mode` (`string`) — режим работы термостата: `"OFF"`, `"COOL"`, `"HEAT"`, `"AUTO"`.

---

### 2.2. Карточка состояния устройства (`DeviceStatusResponse`)

Обертка над телеметрией, содержащая метаданные подключения прибора к брокеру.

```json
{
  "deviceName": "Lamp1",
  "deviceType": "Lamp",
  "topic": "home/lamp1",
  "isOnline": true,
  "lastSeenMs": 1762605000123,
  "telemetry": { ...DeviceTelemetrySchema... }
}

```

**Важная логика жизненного цикла для GUI:**

1. При запуске шлюза он подгружает последние известные показатели каждого устройства из SQLite. В этот момент поле `telemetry` и `lastSeenMs` уже заполнены историческими данными (чтобы интерфейс не был пустым), но **`isOnline` равен `false**`.
2. Как только микро-приложение устройства присылает хотя бы один живой пакет по MQTT в текущей сессии шлюза, флаг **`isOnline` становится `true**` и остается таким до перезапуска шлюза.
3. Если устройство вообще ни разу в жизни не запускалось и записей в БД нет, поле `telemetry` и `lastSeenMs` будут равны `null`, а `isOnline` — `false`.

---

## 3. Системные эндпоинты (`System`)

### `GET /api/v1/health` (алиас: `GET /health`)

Проверка работоспособности шлюза, статуса подключения к MQTT-брокеру Mosquitto, наличия файла БД SQLite и количества активных клиентов.

* **Параметры запроса:** нет.
* **Код успеха:** `200 OK`
* **Пример ответа:**

```json
{
  "status": "ok",
  "service": "Smart Home API Gateway",
  "version": "1.0.0",
  "mqtt_broker": "localhost:1883",
  "mqtt_connected": true,
  "sqlite_db_path": "/home/kirill/ppois-project/smart_home_project/data/smart_home.db",
  "sqlite_db_exists": true,
  "telemetry_records_count": 482,
  "rules_config_path": "/home/kirill/ppois-project/smart_home_project/data/rules.json",
  "registered_devices_count": 5,
  "online_devices": ["Lamp1", "Therm1"],
  "active_ws_clients": 1
}

```

---

## 4. Управление устройствами (`Devices`)

### 4.1. `GET /api/v1/devices`

Возвращает список всех зарегистрированных устройств с их текущим состоянием из оперативного кэша шлюза. Используется для первичной отрисовки главного дашборда в Qt.

* **Параметры запроса:** нет.
* **Код успеха:** `200 OK`
* **Формат ответа:** JSON-массив объектов `DeviceStatusResponse`.

```json
[
  {
    "deviceName": "Lamp1",
    "deviceType": "Lamp",
    "topic": "home/lamp1",
    "isOnline": true,
    "lastSeenMs": 1762605010500,
    "telemetry": {
      "deviceName": "Lamp1",
      "deviceType": "Lamp",
      "serialNumber": null,
      "timestamp": 1762605010500,
      "timestampIso": "2025-11-08T12:30:10.500000+00:00",
      "isOn": true,
      "currentTemperature": 0.0,
      "targetTemperature": 0.0,
      "currentHumidity": 0.0,
      "currentPower": 0.0,
      "brightness": 85,
      "valveLevel": 0,
      "targetValveLevel": 0,
      "currentValveLevel": 0,
      "mode": "OFF"
    }
  }
]

```

---

### 4.2. `GET /api/v1/devices/{device_name}`

Возвращает карточку состояния одного конкретного устройства по его системному имени.

* **Path-параметры:**
* `device_name` (`string`, обязательный) — имя прибора, чувствительное к регистру (например, `Lamp1`, `Plug1`, `Rad1`, `Therm1`, `Hygro1`).


* **Коды ответа:**
* `200 OK` — возвращает объект `DeviceStatusResponse`.
* `404 Not Found` — устройство с таким именем не зарегистрировано:
```json
{
  "detail": "Device 'UnknownDev' not found"
}

```





---

### 4.3. `POST /api/v1/devices/{device_name}/command`

Валидирует и отправляет команду управления конкретному устройству через MQTT-топик `<topic>/set` (с `QoS = 1`).

* **Path-параметры:**
* `device_name` (`string`, обязательный) — имя целевого устройства (`Lamp1`, `Plug1`, `Rad1`, `Therm1`, `Hygro1`).


* **Тело запроса (`CommandRequest`):**
```json
{
  "command": "SET_BRIGHTNESS",
  "value": 75,
  "mode": null
}

```



#### Справочник допустимых команд и правил валидации:

| Значение `command` | Целевые устройства | Обязательные поля в JSON | Примечание для разработчика GUI |
| --- | --- | --- | --- |
| `"ON"` | Все устройства | Только `"command"` | Включает устройство |
| `"OFF"` | Все устройства | Только `"command"` | Выключает устройство |
| `"SET_BRIGHTNESS"` | `Lamp1` | `"command"`, `"value"` (`int`/`float`) | Приводится к `int` (штатный диапазон `0..100`, C++ ядро делает `std::clamp`) |
| `"SET_VALVE"` или `"SET_VALVE_LEVEL"` | `Rad1` | `"command"`, `"value"` (`int`/`float`) | Уровень открытия клапана `0..100` |
| `"SET_TEMP"` или `"SET_TARGET_TEMP"` | `Therm1` | `"command"`, `"value"` (`float`) | Целевая температура в °C (например, `22.5`) |
| `"SET_MODE"` | `Therm1` | `"command"`, `"mode"` (`string`) | Допустимые значения `mode`: `"OFF"`, `"COOL"`, `"HEAT"`, `"AUTO"` |
| `"SET_POWER"` | `Plug1` | `"command"`, `"value"` (`float`) | Установка нагрузки в Ваттах |
| `"SET_HUMIDITY"` | `Hygro1` | `"command"`, `"value"` (`float`) | Установка влажности в `%` |

* **Примеры валидных тел запроса:**
* Включить лампу: `{"command": "ON"}`
* Изменить яркость лампы: `{"command": "SET_BRIGHTNESS", "value": 80}`
* Переключить режим термостата: `{"command": "SET_MODE", "mode": "HEAT"}`
* Задать целевую температуру термостата: `{"command": "SET_TARGET_TEMP", "value": 23.5}`


* **Успешный ответ (`200 OK`, схема `CommandResponse`):**
```json
{
  "status": "sent",
  "deviceName": "Lamp1",
  "topic": "home/lamp1/set",
  "payload": {
    "command": "SET_BRIGHTNESS",
    "value": 80
  }
}

```


* **Коды ошибок:**
* `404 Not Found` — устройство не найдено.
* `422 Unprocessable Entity` — ошибка валидации Pydantic (например, передана команда `"SET_MODE"` без поля `"mode"`, или `"SET_BRIGHTNESS"` без `"value"`, или неизвестная команда).
* `503 Service Unavailable` — шлюз в данный момент не подключен к MQTT-брокеру Mosquitto (нужно отобразить пользователю уведомление об отсутствии связи с брокером):
```json
{
  "detail": "MQTT broker is currently unavailable. Command cannot be delivered."
}

```





---

## 5. История телеметрии и графики (`Telemetry History`)

Данные читаются напрямую из SQLite-таблицы `telemetry_history` в неблокирующем режиме `WAL`.

### 5.1. `GET /api/v1/devices/{device_name}/history`

Универсальный эндпоинт для получения истории конкретного прибора. Имеет **два режима работы** в зависимости от переданных query-параметров:

1. **Режим лога / таблицы (без `start_ts` и `end_ts`):** возвращает последние `limit` записей, отсортированные **от новых к старым (`DESC`)**.
2. **Режим графика для QtCharts (переданы оба параметра `start_ts` и `end_ts`):** возвращает записи, попавшие в интервал `[start_ts, end_ts]`, отсортированные **хронологически от старых к новым (`ASC`)**, чтобы точки можно было сразу добавлять на ось времени слева направо (`QLineSeries::append(timestamp, value)`).

* **Path-параметры:**
* `device_name` (`string`, обязательный) — имя устройства (например, `Therm1`).


* **Query-параметры:**
* `limit` (`int`, опциональный, по умолчанию `100`, допустимый диапазон `1..5000`) — максимальное количество возвращаемых точек.
* `start_ts` (`int64`, опциональный, `>= 0`) — начало интервала времени в Unix **миллисекундах**.
* `end_ts` (`int64`, опциональный, `>= 0`) — конец интервала времени в Unix **миллисекундах**.


* **Примеры запросов:**
* Последние 50 записей для таблицы: `GET /api/v1/devices/Therm1/history?limit=50`
* Точки для графика за интервал времени: `GET /api/v1/devices/Therm1/history?start_ts=1762600000000&end_ts=1762605000000&limit=1000`


* **Коды ответа:**
* `200 OK` — массив объектов `DeviceTelemetrySchema`.
* `400 Bad Request` — если передан только один из параметров (`start_ts` без `end_ts` или наоборот) либо если `start_ts > end_ts`.
* `404 Not Found` — устройство неизвестно ни кэшу, ни базе данных.



---

### 5.2. `GET /api/v1/history`

Возвращает общую ленту последних записей телеметрии по всем устройствам дома сразу (отсортированную по убыванию времени `DESC`). Удобно для общей вкладки «Журнал событий».

* **Query-параметры:**
* `limit` (`int`, опциональный, по умолчанию `100`, диапазон `1..5000`).


* **Код успеха:** `200 OK` (массив объектов `DeviceTelemetrySchema`).

---

## 6. Правила автоматизации и таймеры (`Automation Rules`)

Эндпоинты управляют файлом `data/rules.json`, который используется C++ классом `RuleConfigParser` и движком `RuleEngine`. Все операции записи защищены потоковой блокировкой (`threading.Lock`) и не сохраняют `null`-поля, оставляя чистый JSON для C++.

### Схема одного правила (`RuleItemSchema`)

Поддерживаются 4 типа таймеров (`type`), заложенных в C++ ядре:

1. `"point"` — суточный таймер на конкретное время `HH:MM` или `HH:MM:SS` (поля: `time`, `repeatDaily`).
2. `"timestamp"` — однократный таймер на конкретную дату/время или Unix timestamp (поле: `timestamp`).
3. `"time_window"` — интервальное правило с временем начала, окончания и действием при выходе из окна (поля: `startTime`, `endTime`, `action`, `endAction`, `repeatDaily`).
4. `"periodic"` — периодический запуск каждые N секунд/минут (поля: `intervalMs` или `intervalSeconds`, `maxRuns`).

```json
{
  "id": "morning_warmup",
  "name": "Утренний прогрев спальни",
  "type": "time_window",
  "enabled": true,
  "startTime": "07:00:00",
  "endTime": "09:00:00",
  "repeatDaily": true,
  "action": {
    "device": "Therm1",
    "command": {
      "command": "SET_TARGET_TEMP",
      "value": 23.5
    }
  },
  "endAction": {
    "device": "Therm1",
    "command": {
      "command": "SET_TARGET_TEMP",
      "value": 19.0
    }
  }
}

```

*(Примечание: внутри `action` / `endAction` обязательно должно быть указано либо поле `"device"` — например, `"Lamp1"`, либо прямой MQTT-топик `"topic"` — например, `"home/lamp1/set"`).*

---

### 6.1. `GET /api/v1/rules`

Читает и возвращает весь текущий список правил из `data/rules.json`.

* **Код успеха:** `200 OK`
* **Пример ответа (`RulesConfigSchema`):**

```json
{
  "rules": [
    {
      "id": "night_lamp_off",
      "type": "point",
      "enabled": true,
      "time": "23:30:00",
      "repeatDaily": true,
      "action": {
        "device": "Lamp1",
        "command": {
          "command": "OFF"
        }
      }
    }
  ]
}

```

---

### 6.2. `PUT /api/v1/rules`

Полностью перезаписывает файл `data/rules.json` новым списком правил.

* **Тело запроса (`RulesConfigSchema`):** `{"rules": [ ... ]}`
* **Коды ответа:**
* `200 OK` — возвращает сохраненный объект `RulesConfigSchema`.
* `400 Bad Request` — если в массиве `rules` встретились два правила с одинаковым `id`:
```json
{
  "detail": "Duplicate rule id 'night_lamp_off' in payload."
}

```


* `422 Unprocessable Entity` — ошибка структуры правила (например, в `action` не указан ни `device`, ни `topic`).



---

### 6.3. `POST /api/v1/rules/items`

Точечно добавляет одно новое правило в конец списка в `data/rules.json`.

* **Тело запроса:** объект `RuleItemSchema`.
* **Коды ответа:**
* `201 Created` — правило успешно добавлено, в ответе возвращается созданный объект `RuleItemSchema`.
* `409 Conflict` — правило с таким `id` уже существует в файле:
```json
{
  "detail": "Rule with id 'night_lamp_off' already exists."
}

```


* `422 Unprocessable Entity` — ошибка валидации полей.



---

### 6.4. `DELETE /api/v1/rules/items/{rule_id}`

Точечно удаляет правило по его `id` из `data/rules.json`.

* **Path-параметры:**
* `rule_id` (`string`, обязательный) — идентификатор удаляемого правила.


* **Коды ответа:**
* `200 OK` — возвращает обновленный объект `RulesConfigSchema` (`{"rules": [...]}`) с оставшимися правилами.
* `404 Not Found` — правило с указанным `rule_id` не найдено.



---

## 7. Потоковая телеметрия в реальном времени (`WebSocket`)

### `WS /ws/telemetry` (алиас: `WS /api/v1/ws/telemetry`)

Однонаправленный (Read-Only) канал для мгновенного получения обновлений телеметрии в графическом интерфейсе без постоянного HTTP-опроса (polling).

* **Query-параметры:**
* `device` (`string`, опциональный) — фильтр по имени устройства.
* Если подключиться к `ws://127.0.0.1:8001/ws/telemetry` — клиент будет получать пакеты от **всех** устройств дома.
* Если подключиться к `ws://127.0.0.1:8001/ws/telemetry?device=Therm1` — клиент будет получать пакеты **только** от `Therm1`.





### Протокол работы WebSocket-соединения:

1. **Начальный снимок (Initial Snapshot) сразу после подключения:**
Как только соединение установлено (`QWebSocket::connected`), сервер **сразу же** отправляет клиенту серию отдельных JSON-сообщений для каждого известного устройства в кэше (у которого есть данные из БД или MQTT).
2. **Трансляция живых событий (Live Broadcast):**
Каждый раз, когда любое C++ микро-приложение публикует состояние в MQTT (`home/#`), шлюз валидирует пакет и мгновенно пересылает его во все открытые WebSocket-сессии.
3. **Формат каждого входящего сообщения от сервера:**
Каждое сообщение — это **плоский JSON-объект `DeviceTelemetrySchema**` (без дополнительных конвертов или вложенностей, точно такой же формат, как в `GET /history`):
```json
{
  "deviceName": "Lamp1",
  "deviceType": "Lamp",
  "serialNumber": null,
  "timestamp": 1762605099000,
  "timestampIso": "2025-11-08T12:31:39+00:00",
  "isOn": true,
  "currentTemperature": 0.0,
  "targetTemperature": 0.0,
  "currentHumidity": 0.0,
  "currentPower": 0.0,
  "brightness": 95,
  "valveLevel": 0,
  "targetValveLevel": 0,
  "currentValveLevel": 0,
  "mode": "OFF"
}

```


4. **Проверка связи (Keepalive Ping/Pong):**
Клиент может в любой момент отправить текстовый фрейм `"ping"` — сервер немедленно ответит текстовым фреймом `"pong"`.

---

## 8. Рекомендуемый алгоритм интеграции для второго разработчика (C++/Qt)

1. **При открытии главного окна Qt-приложения:**
* Выполнить `GET [http://127.0.0.1:8001/api/v1/devices](http://127.0.0.1:8001/api/v1/devices)` — создать виджеты карточек всех 5 устройств, выставить им статус `Online/Offline` (`isOnline`) и начальные значения слайдеров/индикаторов из поля `telemetry`.
* Открыть постоянное соединение `QWebSocket` на `ws://127.0.0.1:8001/ws/telemetry`. При приходе каждого JSON-сообщения читать поле `"deviceName"`, находить соответствующую карточку на экране, переводить её индикатор сети в `Online` и обновлять показания датчиков.


2. **При нажатии кнопки или перемещении слайдера пользователем:**
* Отправлять `POST [http://127.0.0.1:8001/api/v1/devices/](http://127.0.0.1:8001/api/v1/devices/){deviceName}/command` с JSON-командой (например, `{"command": "SET_BRIGHTNESS", "value": 60}`).
* Если вернулся код `503`, показать всплывающее предупреждение «MQTT-брокер недоступен». Если `200 OK` — само C++ устройство выполнит команду и пришлет новое состояние обратно через MQTT -> WebSocket.


3. **При переходе на вкладку «Графики / История»:**
* Для отрисовки кривой за выбранный период вызывать `GET /api/v1/devices/{deviceName}/history?start_ts=...&end_ts=...&limit=500` (точки придут уже отсортированными по возрастанию времени `ASC`).


4. **При переходе на вкладку «Правила и Таймеры»:**
* Загружать список через `GET /api/v1/rules`, добавлять новые правила через `POST /api/v1/rules/items` и удалять через `DELETE /api/v1/rules/items/{id}`.
