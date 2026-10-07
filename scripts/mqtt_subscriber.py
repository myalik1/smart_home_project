"""
MQTT Subscriber — читает сообщения из брокера.

Запуск:
    python scripts/mqtt_subscriber.py
"""

import paho.mqtt.client as mqtt
import sys
import time

BROKER = "localhost"

PORT = 1883

TOPIC = "home/test"

CLIENT_ID = "smart_home_subscriber_test"

try:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CLIENT_ID)
except AttributeError:
    client = mqtt.Client(client_id=CLIENT_ID)


def on_connect(client, userdata, flags, rc, properties=None):
    """Вызывается при подключении к брокеру."""
    if rc == 0:
        print(f"[OK] Подключено к {BROKER}:{PORT}")
        client.subscribe(TOPIC, qos=1)
        print(f"[OK] Подписка на '{TOPIC}' активна")
        print("Ожидаю сообщения... (Ctrl+C для выхода)\n")
    else:
        print(f"[ERROR] Не удалось подключиться, код: {rc}")
        sys.exit(1)


def on_message(client, userdata, msg):
    """Вызывается при получении сообщения из топика."""
    timestamp = time.strftime("%H:%M:%S")
    payload = msg.payload.decode("utf-8", errors="replace")
    print(f"[{timestamp}] Топик: {msg.topic}")
    print(f"           Данные: {payload}\n")


def on_disconnect(client, userdata, *args):
    print("[INFO] Отключено")


client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect

print(f"Подключение к {BROKER}:{PORT}...")
try:
    client.connect(BROKER, PORT, keepalive=60)
except Exception as e:
    print(f"[ERROR] Не могу подключиться: {e}")
    sys.exit(1)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n[INFO] Выход по Ctrl+C")
    client.disconnect()