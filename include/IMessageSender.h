#pragma once
#include <string>

// Interface for sending messages to a network (e.g., an MQTT broker)
class IMessageSender {
public:
    virtual ~IMessageSender() = default;

    // Sends a string message to a specific topic
    virtual void sendMessage(const std::string& topic, const std::string& message) = 0;
};
