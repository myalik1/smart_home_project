"""
MQTT Publisher — отправляет одно сообщение в брокер.

Запуск:
    python scripts/mqtt_publisher.py
    python scripts/mqtt_publisher.py "23.5"
    python scripts/mqtt_publisher.py "23.5" "home/kitchen/temperature"
"""

import paho.mqtt.client as mqtt
import sys
import time



BROKER = "localhost"

PORT = 1883


TOPIC = "home/test"


CLIENT_ID = "smart_home_publisher_test"

MESSAGE = sys.argv[1] if len(sys.argv) > 1 else "hello from python publisher"

if len(sys.argv) > 2:
    TOPIC = sys.argv[2]


try:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CLIENT_ID)
except AttributeError:
    client = mqtt.Client(client_id=CLIENT_ID)


def on_connect(client, userdata, flags, rc, properties=None):
    """Вызывается при подключении. Сразу отправляем сообщение."""
    if rc == 0:
        print(f"[OK] Подключено к {BROKER}:{PORT}")

        result = client.publish(TOPIC, MESSAGE, qos=1)
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"[OK] Отправлено в '{TOPIC}': {MESSAGE}")
        else:
            print(f"[ERROR] Ошибка публикации: rc={result.rc}")

        time.sleep(0.5)
        client.disconnect()
    else:
        print(f"[ERROR] Не удалось подключиться, код: {rc}")
        sys.exit(1)


def on_disconnect(client, userdata, *args):
    print("[INFO] Отключено")


client.on_connect = on_connect
client.on_disconnect = on_disconnect

print(f"Подключение к {BROKER}:{PORT}...")
try:
    client.connect(BROKER, PORT, keepalive=60)
except Exception as e:
    print(f"[ERROR] Не могу подключиться: {e}")
    sys.exit(1)

client.loop_forever()
print("[INFO] Завершено")