#pragma once

#include "IMessageSender.h"
#include <string>
#include <functional>

using MqttMessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

class IMqttClient : public IMessageSender {
public:
    virtual ~IMqttClient() = default;
    virtual void subscribe(const std::string& topic, MqttMessageCallback callback) = 0;
};
