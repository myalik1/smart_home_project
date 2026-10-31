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
        bool handled = false;
        auto it = subscriptions.find(topic);
        if (it != subscriptions.end() && it->second) {
            it->second(topic, payload);
            handled = true;
        }

        // Support MQTT wildcard '#'
        for (const auto& [subTopic, cb] : subscriptions) {
            if (subTopic == topic) continue;

            if (subTopic == "#") {
                if (cb) { cb(topic, payload); handled = true; }
            } else if (subTopic.size() >= 2 && subTopic.substr(subTopic.size() - 2) == "/#") {
                std::string prefix = subTopic.substr(0, subTopic.size() - 2);
                if (topic == prefix || (topic.rfind(prefix + "/", 0) == 0)) {
                    if (cb) { cb(topic, payload); handled = true; }
                }
            }
        }
        return handled;
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
