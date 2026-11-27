#ifndef SMARTHOME_GUI_NET_DTOMODELS_H
#define SMARTHOME_GUI_NET_DTOMODELS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace smarthome::net {

/**
 * @brief Telemetry snapshot for a smart home device (matches DeviceTelemetrySchema).
 */
struct DeviceTelemetry {
    QString deviceName;
    QString deviceType = QStringLiteral("Unknown");
    QString serialNumber;
    qint64 timestamp = 0;
    QString timestampIso;

    bool isOn = false;
    double currentTemperature = 0.0;
    double targetTemperature = 0.0;
    double currentHumidity = 0.0;
    double currentPower = 0.0;
    int brightness = 0;
    int valveLevel = 0;
    int targetValveLevel = 0;
    int currentValveLevel = 0;
    QString mode = QStringLiteral("OFF");

    bool isValid() const { return !deviceName.isEmpty(); }

    static DeviceTelemetry fromJson(const QJsonObject &obj);
    static std::optional<DeviceTelemetry> fromJsonBytes(const QByteArray &bytes, QString *errorOut = nullptr);
    static QVector<DeviceTelemetry> listFromJson(const QJsonArray &arr);
    QJsonObject toJson() const;
};

/**
 * @brief Device status card including online state and latest telemetry (matches DeviceStatusResponse).
 */
struct DeviceStatus {
    QString deviceName;
    QString deviceType;
    QString topic;
    bool isOnline = false;
    std::optional<qint64> lastSeenMs;
    std::optional<DeviceTelemetry> telemetry;

    static DeviceStatus fromJson(const QJsonObject &obj);
    static QVector<DeviceStatus> listFromJson(const QJsonArray &arr);
    QJsonObject toJson() const;
};

/**
 * @brief Target device/topic and command payload inside an automation rule.
 */
struct RuleAction {
    std::optional<QString> device;
    std::optional<QString> topic;
    QJsonValue command;

    static RuleAction fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
};

/**
 * @brief Automation rule or timer definition (matches RuleItemSchema).
 */
struct RuleItem {
    QString id;
    QString type = QStringLiteral("point");
    bool enabled = true;

    std::optional<QString> time;
    std::optional<bool> repeatDaily;

    std::optional<qint64> timestamp;

    std::optional<QString> startTime;
    std::optional<QString> endTime;
    std::optional<bool> maintainState;
    std::optional<int> maintainIntervalSeconds;
    std::optional<RuleAction> startAction;
    std::optional<RuleAction> endAction;

    std::optional<int> intervalMs;
    std::optional<int> intervalSeconds;
    std::optional<int> maxTicks;

    std::optional<RuleAction> action;

    static RuleItem fromJson(const QJsonObject &obj);
    static QVector<RuleItem> listFromRulesResponse(const QJsonObject &obj);
    QJsonObject toJson() const;
};

/**
 * @brief Gateway health diagnostics returned by GET /api/v1/health.
 */
struct HealthStatus {
    QString status;
    QString service;
    QString version;
    QString mqttBroker;
    bool mqttConnected = false;
    QString sqliteDbPath;
    bool sqliteDbExists = false;
    int telemetryRecordsCount = 0;
    QString rulesConfigPath;
    int registeredDevicesCount = 0;
    QStringList onlineDevices;
    int activeWsClients = 0;

    static HealthStatus fromJson(const QJsonObject &obj);
};

/**
 * @brief Command payload sent to POST /api/v1/devices/{deviceName}/command.
 */
struct CommandRequest {
    QString command;
    std::optional<double> value;
    std::optional<QString> mode;

    QJsonObject toJson() const;
};

/**
 * @brief Gateway confirmation returned after dispatching a device command.
 */
struct CommandResult {
    QString status;
    QString deviceName;
    QString topic;
    QJsonObject payload;

    static CommandResult fromJson(const QJsonObject &obj);
};

void registerDtoMetaTypes();

} // namespace smarthome::net

Q_DECLARE_METATYPE(smarthome::net::DeviceTelemetry)
Q_DECLARE_METATYPE(QVector<smarthome::net::DeviceTelemetry>)
Q_DECLARE_METATYPE(smarthome::net::DeviceStatus)
Q_DECLARE_METATYPE(QVector<smarthome::net::DeviceStatus>)
Q_DECLARE_METATYPE(smarthome::net::RuleItem)
Q_DECLARE_METATYPE(QVector<smarthome::net::RuleItem>)
Q_DECLARE_METATYPE(smarthome::net::HealthStatus)
Q_DECLARE_METATYPE(smarthome::net::CommandResult)

#endif // SMARTHOME_GUI_NET_DTOMODELS_H
