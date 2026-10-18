#pragma once

#include "../include/IMqttClient.h"
#include <string>
#include <vector>
#include <map>

class MockMqttClient : public IMqttClient {
public:
    std::string lastTopic;
    std::string lastMessage;

    struct SentMessage {
        std::string topic;
        std::string payload;
    };
    std::vector<SentMessage> sentMessages;

    std::map<std::string, MqttMessageCallback> subscriptions;

    void sendMessage(const std::string& topic, const std::string& message) override {
        lastTopic = topic;
        lastMessage = message;
        sentMessages.push_back({topic, message});
    }

    void subscribe(const std::string& topic, MqttMessageCallback callback) override {
        subscriptions[topic] = callback;
    }

    bool simulateIncomingMessage(const std::string& topic, const std::string& payload) {
        auto it = subscriptions.find(topic);
        if (it != subscriptions.end() && it->second) {
            it->second(topic, payload);
            return true;
        }
        return false;
    }

    bool isSubscribed(const std::string& topic) const {
        return subscriptions.find(topic) != subscriptions.end();
    }

    void clear() {
        lastTopic.clear();
        lastMessage.clear();
        sentMessages.clear();
        subscriptions.clear();
    }
};
