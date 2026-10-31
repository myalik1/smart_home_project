#include "../include/MqttMessageSender.h"
#include <iostream>
#include <vector>

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
    {
        std::lock_guard<std::mutex> lock(subscribersMutex);
        subscribers[topic] = std::move(callback);
    }

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

    std::vector<std::string> topicsToSub;
    {
        std::lock_guard<std::mutex> lock(subscribersMutex);
        for (const auto& [topic, _] : subscribers) {
            topicsToSub.push_back(topic);
        }
    }

    for (const auto& topic : topicsToSub) {
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

bool MqttMessageSender::topicMatches(const std::string& pattern, const std::string& topic) {
    if (pattern == topic || pattern == "#") return true;

    auto tokenize = [](const std::string& s) {
        std::vector<std::string> tokens;
        size_t start = 0, end = 0;
        while ((end = s.find('/', start)) != std::string::npos) {
            tokens.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        tokens.push_back(s.substr(start));
        return tokens;
    };

    auto patTokens = tokenize(pattern);
    auto topTokens = tokenize(topic);

    size_t i = 0;
    for (; i < patTokens.size(); ++i) {
        if (patTokens[i] == "#") {
            return true;
        }
        if (i >= topTokens.size()) {
            return false;
        }
        if (patTokens[i] != "+" && patTokens[i] != topTokens[i]) {
            return false;
        }
    }
    return i == topTokens.size();
}

void MqttMessageSender::message_arrived(mqtt::const_message_ptr msg) {
    const std::string topic = msg->get_topic();
    const std::string payload = msg->to_string();

    std::vector<std::function<void(const std::string&, const std::string&)>> matchingCallbacks;
    {
        std::lock_guard<std::mutex> lock(subscribersMutex);

        auto it = subscribers.find(topic);
        if (it != subscribers.end() && it->second) {
            matchingCallbacks.push_back(it->second);
        }

        for (const auto& [pattern, cb] : subscribers) {
            if (pattern == topic) continue;
            if (topicMatches(pattern, topic) && cb) {
                matchingCallbacks.push_back(cb);
            }
        }
    }

    if (!matchingCallbacks.empty()) {
        for (const auto& cb : matchingCallbacks) {
            cb(topic, payload);
        }
    } else {
        std::cout << "[MQTT <-] Сообщение в неизвестный топик '" << topic
                  << "': " << payload << std::endl;
    }
}
