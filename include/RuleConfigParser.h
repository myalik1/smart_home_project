#pragma once

#include "RuleEngine.h"
#include "RuleAction.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <memory>

class RuleConfigParser {
public:
    RuleConfigParser() = default;
    virtual ~RuleConfigParser() = default;

    // Loads and registers rules from a JSON file into RuleEngine
    bool loadFromFile(const std::string& filePath, RuleEngine& engine);

    // Loads and registers rules from a JSON string into RuleEngine
    bool loadFromString(const std::string& jsonContent, RuleEngine& engine);

    // Saves current engine rules into a formatted JSON file
    bool saveToFile(const std::string& filePath, const RuleEngine& engine) const;

    // Serializes rules from engine into JSON string
    std::string exportToJson(const RuleEngine& engine) const;

    std::string getLastError() const;
    size_t getLastLoadedCount() const;

private:
    std::string lastError_;
    size_t lastLoadedCount_ = 0;

    static bool parseTimeStr(const std::string& timeStr, int& outHour, int& outMinute, int& outSecond);
    static bool parseAction(const nlohmann::json& actionJson, RuleAction& outAction);
};
