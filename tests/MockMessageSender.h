#pragma once
#include "../include/IMessageSender.h"
#include <string>

class MockMessageSender : public IMessageSender {
public:
    std::string lastTopic;
    std::string lastMessage;

    void sendMessage(const std::string& topic, const std::string& message) override {
        lastTopic = topic;
        lastMessage = message;
    }

    void clear() {
        lastTopic.clear();
        lastMessage.clear();
    }
};
