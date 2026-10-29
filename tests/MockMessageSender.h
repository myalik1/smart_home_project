#pragma once
#include "../include/IMessageSender.h"
#include <string>
#include <map>

class MockMessageSender : public IMessageSender {
public:
    std::string lastTopic;
    std::string lastMessage;
    std::map<std::string, std::string> topicMessages;
    int callCount = 0;

    void sendMessage(const std::string& topic, const std::string& message) override {
        lastTopic = topic;
        lastMessage = message;
        topicMessages[topic] = message;
        callCount++;
    }

    void clear() {
        lastTopic.clear();
        lastMessage.clear();
        topicMessages.clear();
        callCount = 0;
    }
};
