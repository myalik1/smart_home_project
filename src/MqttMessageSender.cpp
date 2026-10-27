#include "../include/MqttMessageSender.h"
#include <iostream>

MqttMessageSender::MqttMessageSender(const std::string& brokerAddress,
                                     const std::string& clientId_)
    : clientId(clientId_)
{
    client = std::make_unique<mqtt::async_client>(brokerAddress, clientId);
    client->set_callback(*this);
}

MqttMessageSender::~MqttMessageSender() {
    try {
        if (client && client->is_connected()) {
            client->disconnect()->wait();
        }
    } catch (...) {
    }
}

void MqttMessageSender::connect() {
    auto options = mqtt::connect_options_builder()
        .automatic_reconnect(true)
        .clean_session(true)
        .finalize();

    try {
        std::cout << "[MQTT] Подключение к брокеру... (clientId=" << clientId << ")" << std::endl;
        client->connect(options)->wait();
        isConnected = true;
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT ERROR] Не удалось подключиться: " << e.what() << std::endl;
        isConnected = false;
        throw;
    }
}

void MqttMessageSender::disconnect() {
    try {
        if (client && client->is_connected()) {
            client->disconnect()->wait();
        }
        isConnected = false;
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT ERROR] Ошибка при отключении: " << e.what() << std::endl;
    }
}

void MqttMessageSender::sendMessage(const std::string& topic,
                                    const std::string& message) {
    if (!isConnected) {
        std::cerr << "[MQTT] Не подключён — сообщение в '" << topic
                  << "' не отправлено" << std::endl;
        return;
    }

    try {
        auto pubmsg = mqtt::make_message(topic, message);
        pubmsg->set_qos(1);
        client->publish(pubmsg);
        std::cout << "[MQTT ->] " << topic << ": " << message << std::endl;
    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTT ERROR] Ошибка публикации: " << e.what() << std::endl;
    }
}

void MqttMessageSender::subscribe(
    const std::string& topic,
    std::function<void(const std::string&, const std::string&)> callback)
{
    subscribers[topic] = std::move(callback);

    if (isConnected) {
        try {
            client->subscribe(topic, 1);
            std::cout << "[MQTT] Подписка на '" << topic << "' активна" << std::endl;
        } catch (const mqtt::exception& e) {
            std::cerr << "[MQTT ERROR] Ошибка подписки на '" << topic
                      << "': " << e.what() << std::endl;
        }
    }
}

void MqttMessageSender::connected(const std::string& /*cause*/) {
    std::cout << "[MQTT] Соединение установлено" << std::endl;
    isConnected = true;

    for (const auto& [topic, _] : subscribers) {
        try {
            client->subscribe(topic, 1);
            std::cout << "[MQTT] Подписка на '" << topic << "' активна" << std::endl;
        } catch (const mqtt::exception& e) {
            std::cerr << "[MQTT ERROR] Переподписка на '" << topic
                      << "': " << e.what() << std::endl;
        }
    }
}

void MqttMessageSender::connection_lost(const std::string& cause) {
    std::cout << "[MQTT] Связь потеряна: " << cause << std::endl;
    isConnected = false;
}

void MqttMessageSender::message_arrived(mqtt::const_message_ptr msg) {
    const std::string topic = msg->get_topic();
    const std::string payload = msg->to_string();

    auto it = subscribers.find(topic);
    if (it != subscribers.end() && it->second) {
        it->second(topic, payload);
    } else {
        std::cout << "[MQTT <-] Сообщение в неизвестный топик '" << topic
                  << "': " << payload << std::endl;
    }
}