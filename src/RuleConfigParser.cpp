#include "../include/RuleConfigParser.h"
#include "../include/OneShotTimer.h"
#include "../include/IntervalTimer.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <ctime>

using json = nlohmann::json;

namespace {
json serializeAction(const RuleAction& act) {
    json actJson = json::object();
    if (!act.targetDevice.empty()) actJson["device"] = act.targetDevice;
    if (!act.targetTopic.empty()) actJson["topic"] = act.targetTopic;

    try {
        actJson["command"] = json::parse(act.commandJson);
    } catch (...) {
        actJson["command"] = act.commandJson;
    }
    return actJson;
}
} // namespace

bool RuleConfigParser::loadFromFile(const std::string& filePath, RuleEngine& engine) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        lastError_ = "Failed to open rules configuration file: " + filePath;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return loadFromString(buffer.str(), engine);
}

bool RuleConfigParser::loadFromString(const std::string& jsonContent, RuleEngine& engine) {
    lastError_.clear();
    lastLoadedCount_ = 0;

    if (jsonContent.empty()) {
        lastError_ = "Rule configuration JSON is empty";
        return false;
    }

    json root;
    try {
        root = json::parse(jsonContent);
    } catch (const json::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return false;
    }

    json rulesArray;
    if (root.is_array()) {
        rulesArray = root;
    } else if (root.is_object()) {
        if (root.contains("rules") && root["rules"].is_array()) {
            rulesArray = root["rules"];
        } else if (root.contains("timers") && root["timers"].is_array()) {
            rulesArray = root["timers"];
        } else {
            lastError_ = "JSON object must contain a 'rules' or 'timers' array";
            return false;
        }
    } else {
        lastError_ = "Root JSON must be an array or an object with 'rules'/'timers'";
        return false;
    }

    size_t loaded = 0;
    for (size_t i = 0; i < rulesArray.size(); ++i) {
        try {
            const auto& item = rulesArray[i];
            if (!item.is_object()) {
                continue;
            }

            if (!item.contains("id") || !item["id"].is_string()) {
                continue;
            }
            std::string id = item["id"].get<std::string>();

            std::string type = "point";
            if (item.contains("type") && item["type"].is_string()) {
                type = item["type"].get<std::string>();
            }

            bool enabled = true;
            if (item.contains("enabled") && item["enabled"].is_boolean()) {
                enabled = item["enabled"].get<bool>();
            }

            if (type == "point" || type == "time_of_day") {
                if (!item.contains("time") || !item["time"].is_string() || !item.contains("action")) {
                    continue;
                }

                int h = 0, m = 0, s = 0;
                if (!parseTimeStr(item["time"].get<std::string>(), h, m, s)) {
                    continue;
                }

                bool repeatDaily = false;
                if (item.contains("repeatDaily") && item["repeatDaily"].is_boolean()) {
                    repeatDaily = item["repeatDaily"].get<bool>();
                }

                RuleAction action;
                if (!parseAction(item["action"], action)) {
                    continue;
                }

                auto timer = engine.addTimeOfDayTimer(id, h, m, s, action, repeatDaily);
                if (timer) {
                    timer->setEnabled(enabled);
                    loaded++;
                }
            }
            else if (type == "point_absolute" || type == "timestamp") {
                if (!item.contains("timestamp") || !item["timestamp"].is_number() || !item.contains("action")) {
                    continue;
                }

                int64_t ts = item["timestamp"].get<int64_t>();
                std::chrono::system_clock::time_point tp;
                if (ts < 10000000000LL) {
                    tp = std::chrono::system_clock::time_point(std::chrono::seconds(ts));
                } else {
                    tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts));
                }

                RuleAction action;
                if (!parseAction(item["action"], action)) {
                    continue;
                }

                auto timer = engine.addPointTimer(id, tp, action);
                if (timer) {
                    timer->setEnabled(enabled);
                    loaded++;
                }
            }
            else if (type == "time_window" || type == "interval_window") {
                if (!item.contains("startTime") || !item["startTime"].is_string() ||
                    !item.contains("endTime") || !item["endTime"].is_string() ||
                    !item.contains("startAction") || !item.contains("endAction")) {
                    continue;
                }

                int startH = 0, startM = 0, startS = 0;
                int endH = 0, endM = 0, endS = 0;

                if (!parseTimeStr(item["startTime"].get<std::string>(), startH, startM, startS) ||
                    !parseTimeStr(item["endTime"].get<std::string>(), endH, endM, endS)) {
                    continue;
                }

                RuleAction startAct, endAct;
                if (!parseAction(item["startAction"], startAct) || !parseAction(item["endAction"], endAct)) {
                    continue;
                }

                bool repeatDaily = true;
                if (item.contains("repeatDaily") && item["repeatDaily"].is_boolean()) {
                    repeatDaily = item["repeatDaily"].get<bool>();
                }

                bool maintainState = false;
                if (item.contains("maintainState") && item["maintainState"].is_boolean()) {
                    maintainState = item["maintainState"].get<bool>();
                }

                int maintainSec = 60;
                if (item.contains("maintainIntervalSeconds") && item["maintainIntervalSeconds"].is_number()) {
                    maintainSec = item["maintainIntervalSeconds"].get<int>();
                }

                auto timer = engine.addTimeOfDayIntervalTimer(
                    id, startH, startM, startS, endH, endM, endS,
                    startAct, endAct, repeatDaily, maintainState, std::chrono::seconds(maintainSec));

                if (timer) {
                    timer->setEnabled(enabled);
                    loaded++;
                }
            }
            else if (type == "periodic") {
                if (!item.contains("action")) {
                    continue;
                }

                int intervalMs = 1000;
                if (item.contains("intervalMs") && item["intervalMs"].is_number()) {
                    intervalMs = item["intervalMs"].get<int>();
                } else if (item.contains("intervalSeconds") && item["intervalSeconds"].is_number()) {
                    intervalMs = item["intervalSeconds"].get<int>() * 1000;
                }

                size_t maxTicks = 0;
                if (item.contains("maxTicks") && item["maxTicks"].is_number()) {
                    maxTicks = item["maxTicks"].get<size_t>();
                }

                RuleAction action;
                if (!parseAction(item["action"], action)) {
                    continue;
                }

                auto timer = engine.addPeriodicTimer(id, std::chrono::milliseconds(intervalMs), action, maxTicks);
                if (timer) {
                    timer->setEnabled(enabled);
                    loaded++;
                }
            }
        } catch (...) {
            continue;
        }
    }

    lastLoadedCount_ = loaded;
    return true;
}

bool RuleConfigParser::saveToFile(const std::string& filePath, const RuleEngine& engine) const {
    std::string jsonStr = exportToJson(engine);
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    file << jsonStr;
    return true;
}

std::string RuleConfigParser::exportToJson(const RuleEngine& engine) const {
    json root = json::object();
    json rules = json::array();

    auto allTimers = engine.getAllTimers();
    for (const auto& timer : allTimers) {
        if (!timer) continue;

        json item = json::object();
        item["id"] = timer->getId();
        item["enabled"] = timer->isEnabled();

        auto oneShot = std::dynamic_pointer_cast<OneShotTimer>(timer);
        if (oneShot) {
            if (oneShot->isTimeOfDayMode()) {
                item["type"] = "time_of_day";
                char timeBuf[16];
                std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
                              oneShot->getHour(), oneShot->getMinute(), oneShot->getSecond());
                item["time"] = std::string(timeBuf);
                item["repeatDaily"] = oneShot->isRepeatDaily();
            } else {
                item["type"] = "point_absolute";
                auto tsSec = std::chrono::duration_cast<std::chrono::seconds>(oneShot->getTriggerTime().time_since_epoch()).count();
                item["timestamp"] = tsSec;
            }
            item["action"] = serializeAction(oneShot->getAction());
        }

        auto interval = std::dynamic_pointer_cast<IntervalTimer>(timer);
        if (interval) {
            if (interval->getMode() == IntervalTimer::Mode::PERIODIC) {
                item["type"] = "periodic";
                item["intervalMs"] = interval->getPeriodicInterval().count();
                if (interval->getMaxTicks() > 0) {
                    item["maxTicks"] = interval->getMaxTicks();
                }
                item["action"] = serializeAction(interval->getAction());
            } else if (interval->getMode() == IntervalTimer::Mode::TIME_OF_DAY_WINDOW) {
                item["type"] = "time_window";
                char startBuf[16], endBuf[16];
                std::snprintf(startBuf, sizeof(startBuf), "%02d:%02d:%02d",
                              interval->getStartHour(), interval->getStartMin(), interval->getStartSec());
                std::snprintf(endBuf, sizeof(endBuf), "%02d:%02d:%02d",
                              interval->getEndHour(), interval->getEndMin(), interval->getEndSec());
                item["startTime"] = std::string(startBuf);
                item["endTime"] = std::string(endBuf);
                item["repeatDaily"] = interval->isRepeatDaily();
                item["maintainState"] = interval->isMaintainState();
                if (interval->isMaintainState()) {
                    item["maintainIntervalSeconds"] = interval->getMaintainInterval().count();
                }
                item["startAction"] = serializeAction(interval->getStartAction());
                item["endAction"] = serializeAction(interval->getEndAction());
            } else {
                item["type"] = "time_window";
                char startBuf[16], endBuf[16];
                std::time_t sTime = std::chrono::system_clock::to_time_t(interval->getWindowStart());
                std::time_t eTime = std::chrono::system_clock::to_time_t(interval->getWindowEnd());
                std::tm sTm{}, eTm{};
#if defined(_WIN32)
                localtime_s(&sTm, &sTime);
                localtime_s(&eTm, &eTime);
#else
                localtime_r(&sTime, &sTm);
                localtime_r(&eTime, &eTm);
#endif
                std::snprintf(startBuf, sizeof(startBuf), "%02d:%02d:%02d", sTm.tm_hour, sTm.tm_min, sTm.tm_sec);
                std::snprintf(endBuf, sizeof(endBuf), "%02d:%02d:%02d", eTm.tm_hour, eTm.tm_min, eTm.tm_sec);
                item["startTime"] = std::string(startBuf);
                item["endTime"] = std::string(endBuf);
                item["repeatDaily"] = false;
                item["maintainState"] = interval->isMaintainState();
                if (interval->isMaintainState()) {
                    item["maintainIntervalSeconds"] = interval->getMaintainInterval().count();
                }
                item["startAction"] = serializeAction(interval->getStartAction());
                item["endAction"] = serializeAction(interval->getEndAction());
            }
        }

        rules.push_back(std::move(item));
    }

    root["rules"] = std::move(rules);
    return root.dump(2);
}

std::string RuleConfigParser::getLastError() const {
    return lastError_;
}

size_t RuleConfigParser::getLastLoadedCount() const {
    return lastLoadedCount_;
}

bool RuleConfigParser::parseTimeStr(const std::string& timeStr, int& outHour, int& outMinute, int& outSecond) {
    std::stringstream ss(timeStr);
    std::string hStr, mStr, sStr;

    if (!std::getline(ss, hStr, ':')) return false;
    if (!std::getline(ss, mStr, ':')) return false;
    if (!std::getline(ss, sStr, ':')) {
        sStr = "0";
    }

    try {
        int h = std::stoi(hStr);
        int m = std::stoi(mStr);
        int s = std::stoi(sStr);

        if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
            return false;
        }

        outHour = h;
        outMinute = m;
        outSecond = s;
        return true;
    } catch (...) {
        return false;
    }
}

bool RuleConfigParser::parseAction(const json& actionJson, RuleAction& outAction) {
    if (!actionJson.is_object()) {
        return false;
    }

    if (actionJson.contains("device") && actionJson["device"].is_string()) {
        outAction.targetDevice = actionJson["device"].get<std::string>();
    }

    if (actionJson.contains("topic") && actionJson["topic"].is_string()) {
        outAction.targetTopic = actionJson["topic"].get<std::string>();
    }

    if (actionJson.contains("command")) {
        if (actionJson["command"].is_string()) {
            outAction.commandJson = actionJson["command"].get<std::string>();
        } else if (actionJson["command"].is_object()) {
            outAction.commandJson = actionJson["command"].dump();
        } else {
            return false;
        }
    } else {
        return false;
    }

    return !outAction.targetDevice.empty() || !outAction.targetTopic.empty();
}
