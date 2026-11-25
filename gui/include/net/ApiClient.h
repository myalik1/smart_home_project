#ifndef SMARTHOME_GUI_NET_APICLIENT_H
#define SMARTHOME_GUI_NET_APICLIENT_H

#include "net/DtoModels.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>

#include <optional>

class QJsonDocument;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace smarthome::net {

/**
 * @brief Asynchronous REST client for the Smart Home FastAPI gateway.
 */
class ApiClient : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(const QUrl &baseUrl = QUrl(QStringLiteral("http://127.0.0.1:8001/api/v1")),
                       QObject *parent = nullptr);

    QUrl baseUrl() const { return baseUrl_; }
    void setBaseUrl(const QUrl &baseUrl);

    int transferTimeoutMs() const { return transferTimeoutMs_; }
    void setTransferTimeoutMs(int timeoutMs);

    void getHealth();
    void getDevices();
    void sendCommand(const QString &deviceName, const CommandRequest &command);
    void getDeviceHistory(const QString &deviceName,
                          int limit = 100,
                          std::optional<qint64> startTs = std::nullopt,
                          std::optional<qint64> endTs = std::nullopt);
    void getRules();
    void addRule(const RuleItem &rule);
    void toggleRule(const QString &ruleId, bool enabled);
    void deleteRule(const QString &ruleId);

signals:
    void healthReceived(const smarthome::net::HealthStatus &health);
    void devicesReceived(const QVector<smarthome::net::DeviceStatus> &devices);
    void commandSent(const smarthome::net::CommandResult &result);
    void deviceHistoryReceived(const QString &deviceName,
                               const QVector<smarthome::net::DeviceTelemetry> &history);
    void rulesReceived(const QVector<smarthome::net::RuleItem> &rules);
    void ruleAdded(const smarthome::net::RuleItem &rule);
    void ruleToggled(const smarthome::net::RuleItem &rule);
    void ruleDeleted(const QString &ruleId, const QVector<smarthome::net::RuleItem> &remainingRules);

    void requestFailed(const QString &endpoint, int httpStatus, const QString &errorMessage);

private:
    QUrl buildUrl(const QString &relativePath, const QUrlQuery &query = QUrlQuery()) const;
    QNetworkRequest createJsonRequest(const QUrl &url) const;

    template <typename OnSuccess>
    void attachReplyHandler(QNetworkReply *reply, const QString &endpoint, OnSuccess &&onSuccess);

    QString extractErrorMessage(QNetworkReply *reply, const QByteArray &body) const;

    QNetworkAccessManager *networkManager_;
    QUrl baseUrl_;
    int transferTimeoutMs_ = 10000;
};

} // namespace smarthome::net

#endif // SMARTHOME_GUI_NET_APICLIENT_H
