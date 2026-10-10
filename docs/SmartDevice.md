## Оглавление

1. [Базовые интерфейсы и классы](#базовые-интерфейсы-и-классы)
2. [Конкретные устройства и форматы JSON](#конкретные-устройства-и-форматы-json)

## Базовые интерфейсы и классы

### `IMessageSender` (Интерфейс)
Абстрактный интерфейс, обеспечивающий изоляцию сетевой логики (Dependency Inversion).
* **Назначение:** Любой класс, отвечающий за отправку данных в сеть (например, `MqttSender`), должен реализовывать этот интерфейс.
* **Методы:**
  * `virtual void sendMessage(const std::string& topic, const std::string& message) = 0;` — принимает топик маршрутизации и полезную нагрузку (JSON строку) для отправки.

### `SmartDevice` (Абстрактный класс)
Базовый класс для всех умных устройств в системе. Хранит общие метаданные и указатель на интерфейс связи.
* **Поля (protected):**
  * `deviceName` (Имя устройства)
  * `serialNumber` (Уникальный серийный номер)
  * `deviceType` (Тип устройства)
  * `mqttTopic` (Сетевой топик, в который устройство отправляет свой статус)
* **Методы:**
  * `void setSender(IMessageSender* s)` — устанавливает передатчик сообщений.
  * `virtual void publishState() = 0` — устройства реализуют его для упаковки своего текущего состояния в JSON и отправки.
  * `virtual void onMessageReceived(const std::string& message) = 0` — обработка входящих JSON-команд.

## Конкретные устройства и форматы JSON

Все устройства при любом успешном изменении своего состояния автоматически вызывают метод `publishState()`, который отправляет актуальный статус в брокер. Ниже описаны входящие команды и формат отправляемых данных для каждого устройства.

*(Примечание: Все отправляемые JSON-объекты всегда включают базовые поля: `deviceName`, `serialNumber` и `deviceType`)*

### `SmartLamp` (Умная лампочка)
Управляет состоянием питания и яркостью света.

* **Входящие команды (JSON):**
  * `{"command": "ON"}`
  * `{"command": "OFF"}`
  * `{"command": "SET_BRIGHTNESS", "value": 75}`

* **Отправляемые данные (JSON):**
  ```json
  {
    "deviceName": "Lamp1",
    "serialNumber": "L-01",
    "deviceType": "Lamp",
    "isOn": true,
    "brightness": 75
  }
  ```

### `SmartThermostat` (Умный термостат)
Управляет климатом, поддерживает различные режимы и целевую температуру.

* **Входящие команды (JSON):**
  * `{"command": "SET_TEMPERATURE", "value": 22.5}`
  * `{"command": "SET_MODE", "mode": "HEAT"}`
  * `{"command": "UPDATE_CURRENT_TEMP", "value": 21.0}`

* **Отправляемые данные (JSON):**
  ```json
  {
    "deviceName": "Therm1",
    "serialNumber": "T-01",
    "deviceType": "Thermostat",
    "targetTemperature": 22.5,
    "currentTemperature": 21.0,
    "mode": "HEAT"
  }
  ```

### `SmartPlug` (Умная розетка)
Управляет подачей питания и отслеживает текущее потребление электроэнергии.

* **Входящие команды (JSON):**
  * `{"command": "ON"}`
  * `{"command": "OFF"}`
  * `{"command": "UPDATE_POWER", "value": 150.5}`

* **Отправляемые данные (JSON):**
  ```json
  {
    "deviceName": "Plug1",
    "serialNumber": "P-01",
    "deviceType": "Plug",
    "isOn": true,
    "currentPower": 150.5
  }
  ```

### `SmartHygrometer` (Умный гигрометр)
Сенсорное устройство для мониторинга влажности воздуха в помещении.

* **Входящие команды (JSON):**
  * `{"command": "UPDATE_HUMIDITY", "value": 45.5}`

* **Отправляемые данные (JSON):**
  ```json
  {
    "deviceName": "Hygro1",
    "serialNumber": "H-01",
    "deviceType": "Hygrometer",
    "currentHumidity": 45.5
  }
  ```

### `SmartRadiator` (Умная батарея / Радиатор)
Управляет обогревом с помощью контроля целевой температуры и клапана.

* **Входящие команды (JSON):**
  * `{"command": "ON"}`
  * `{"command": "OFF"}`
  * `{"command": "SET_TARGET_TEMP", "value": 24.0}`
  * `{"command": "UPDATE_CURRENT_TEMP", "value": 20.0}`
  * `{"command": "SET_VALVE_LEVEL", "value": 50}`

* **Отправляемые данные (JSON):**
  ```json
  {
    "deviceName": "Rad1",
    "serialNumber": "R-01",
    "deviceType": "Radiator",
    "isOn": true,
    "targetTemperature": 24.0,
    "currentTemperature": 20.0,
    "valveLevel": 50
  }
  ```
