#pragma once
#include "IMessageSender.h"
#include <mqtt/async_client.h>
#include <memory>
#include <functional>
#include <map>
#include <string>

// Реализация IMessageSender через MQTT (Paho).
// Умеет:
//   - отправлять сообщения (IMessageSender::sendMessage)
//   - подписываться на топики и вызывать callback при получении
class MqttMessageSender : public IMessageSender, public virtual mqtt::callback {
public:
    // <<< ЗАМЕНИ ЗДЕСЬ: адрес брокера и уникальный clientId подставишь в main.cpp
    MqttMessageSender(const std::string& brokerAddress,
                      const std::string& clientId);

    ~MqttMessageSender() override;

    void sendMessage(const std::string& topic, const std::string& message) override;

    void connect();
    void disconnect();

    void subscribe(const std::string& topic,
                   std::function<void(const std::string&, const std::string&)> callback);

    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;

private:
    std::unique_ptr<mqtt::async_client> client;
    std::string clientId;
    bool isConnected = false;

    std::map<std::string,
             std::function<void(const std::string&, const std::string&)>> subscribers;
};