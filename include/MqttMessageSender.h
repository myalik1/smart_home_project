#pragma once
#include "IMqttClient.h"
#include <mqtt/async_client.h>
#include <memory>
#include <functional>
#include <map>
#include <string>
#include <mutex>

class MqttMessageSender : public IMqttClient, public virtual mqtt::callback {
public:
    MqttMessageSender(const std::string& brokerAddress,
                      const std::string& clientId);

    ~MqttMessageSender() override;

    void sendMessage(const std::string& topic, const std::string& message) override;

    void connect();
    void disconnect();

    void subscribe(const std::string& topic,
                   MqttMessageCallback callback) override;

    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;

    static bool topicMatches(const std::string& pattern, const std::string& topic);

private:
    std::unique_ptr<mqtt::async_client> client;
    std::string clientId;
    bool isConnected = false;

    mutable std::mutex subscribersMutex;
    std::map<std::string,
             std::function<void(const std::string&, const std::string&)>> subscribers;
};
