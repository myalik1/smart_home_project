#include "net/ApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace smarthome::net {

ApiClient::ApiClient(const QUrl &baseUrl, QObject *parent)
    : QObject(parent)
    , networkManager_(new QNetworkAccessManager(this))
    , baseUrl_(baseUrl) {
    registerDtoMetaTypes();
}

void ApiClient::setBaseUrl(const QUrl &baseUrl) {
    baseUrl_ = baseUrl;
}

void ApiClient::setTransferTimeoutMs(int timeoutMs) {
    transferTimeoutMs_ = timeoutMs;
}

QUrl ApiClient::buildUrl(const QString &relativePath, const QUrlQuery &query) const {
    QUrl url = baseUrl_;
    QString basePath = url.path();
    while (basePath.endsWith(QLatin1Char('/'))) {
        basePath.chop(1);
    }
    QString cleanRelative = relativePath;
    while (cleanRelative.startsWith(QLatin1Char('/'))) {
        cleanRelative.remove(0, 1);
    }
    url.setPath(basePath + QLatin1Char('/') + cleanRelative);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    return url;
}

QNetworkRequest ApiClient::createJsonRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(transferTimeoutMs_);
    return request;
}

QString ApiClient::extractErrorMessage(QNetworkReply *reply, const QByteArray &body) const {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QJsonValue detail = obj.value(QStringLiteral("detail"));
        if (detail.isString()) {
            return detail.toString();
        }
        if (detail.isArray() && !detail.toArray().isEmpty()) {
            return QString::fromUtf8(QJsonDocument(detail.toArray()).toJson(QJsonDocument::Compact));
        }
    }
    if (!body.trimmed().isEmpty()) {
        return QString::fromUtf8(body.trimmed());
    }
    return reply->errorString();
}

template <typename OnSuccess>
void ApiClient::attachReplyHandler(QNetworkReply *reply, const QString &endpoint, OnSuccess &&onSuccess) {
    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint, onSuccess = std::forward<OnSuccess>(onSuccess)]() {
        reply->deleteLater();

        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError || httpStatus >= 400) {
            emit requestFailed(endpoint, httpStatus, extractErrorMessage(reply, body));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            emit requestFailed(endpoint, httpStatus, parseError.errorString());
            return;
        }

        onSuccess(doc, httpStatus);
    });
}

void ApiClient::getHealth() {
    const QString endpoint = QStringLiteral("/health");
    QNetworkReply *reply = networkManager_->get(createJsonRequest(buildUrl(endpoint)));
    attachReplyHandler(reply, endpoint, [this, endpoint](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isObject()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON object from /health"));
            return;
        }
        emit healthReceived(HealthStatus::fromJson(doc.object()));
    });
}

void ApiClient::getDevices() {
    const QString endpoint = QStringLiteral("/devices");
    QNetworkReply *reply = networkManager_->get(createJsonRequest(buildUrl(endpoint)));
    attachReplyHandler(reply, endpoint, [this, endpoint](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isArray()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON array from /devices"));
            return;
        }
        emit devicesReceived(DeviceStatus::listFromJson(doc.array()));
    });
}

void ApiClient::sendCommand(const QString &deviceName, const CommandRequest &command) {
    const QString encodedName = QString::fromUtf8(QUrl::toPercentEncoding(deviceName));
    const QString endpoint = QStringLiteral("/devices/%1/command").arg(encodedName);
    const QByteArray payload = QJsonDocument(command.toJson()).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = networkManager_->post(createJsonRequest(buildUrl(endpoint)), payload);
    attachReplyHandler(reply, endpoint, [this, endpoint](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isObject()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON object from command endpoint"));
            return;
        }
        emit commandSent(CommandResult::fromJson(doc.object()));
    });
}

void ApiClient::getDeviceHistory(const QString &deviceName,
                                 int limit,
                                 std::optional<qint64> startTs,
                                 std::optional<qint64> endTs) {
    const QString encodedName = QString::fromUtf8(QUrl::toPercentEncoding(deviceName));
    const QString endpoint = QStringLiteral("/devices/%1/history").arg(encodedName);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    if (startTs.has_value()) {
        query.addQueryItem(QStringLiteral("start_ts"), QString::number(*startTs));
    }
    if (endTs.has_value()) {
        query.addQueryItem(QStringLiteral("end_ts"), QString::number(*endTs));
    }

    QNetworkReply *reply = networkManager_->get(createJsonRequest(buildUrl(endpoint, query)));
    attachReplyHandler(reply, endpoint, [this, endpoint, deviceName](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isArray()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON array from device history"));
            return;
        }
        emit deviceHistoryReceived(deviceName, DeviceTelemetry::listFromJson(doc.array()));
    });
}

void ApiClient::getRules() {
    const QString endpoint = QStringLiteral("/rules");
    QNetworkReply *reply = networkManager_->get(createJsonRequest(buildUrl(endpoint)));
    attachReplyHandler(reply, endpoint, [this, endpoint](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isObject()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON object from /rules"));
            return;
        }
        emit rulesReceived(RuleItem::listFromRulesResponse(doc.object()));
    });
}

void ApiClient::addRule(const RuleItem &rule) {
    const QString endpoint = QStringLiteral("/rules/items");
    const QByteArray payload = QJsonDocument(rule.toJson()).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = networkManager_->post(createJsonRequest(buildUrl(endpoint)), payload);
    attachReplyHandler(reply, endpoint, [this, endpoint](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isObject()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON object from POST /rules/items"));
            return;
        }
        emit ruleAdded(RuleItem::fromJson(doc.object()));
    });
}

void ApiClient::toggleRule(const QString &ruleId, bool enabled) {
    const QString encodedId = QString::fromUtf8(QUrl::toPercentEncoding(ruleId));
    const QString endpoint = QStringLiteral("/rules/items/%1").arg(encodedId);

    QJsonObject body;
    body.insert(QStringLiteral("enabled"), enabled);
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = networkManager_->sendCustomRequest(createJsonRequest(buildUrl(endpoint)), "PATCH", payload);
    attachReplyHandler(reply, endpoint, [this, endpoint](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isObject()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON object from PATCH /rules/items"));
            return;
        }
        emit ruleToggled(RuleItem::fromJson(doc.object()));
    });
}

void ApiClient::deleteRule(const QString &ruleId) {
    const QString encodedId = QString::fromUtf8(QUrl::toPercentEncoding(ruleId));
    const QString endpoint = QStringLiteral("/rules/items/%1").arg(encodedId);

    QNetworkReply *reply = networkManager_->deleteResource(createJsonRequest(buildUrl(endpoint)));
    attachReplyHandler(reply, endpoint, [this, endpoint, ruleId](const QJsonDocument &doc, int httpStatus) {
        if (!doc.isObject()) {
            emit requestFailed(endpoint, httpStatus, QStringLiteral("Expected JSON object from DELETE /rules/items"));
            return;
        }
        emit ruleDeleted(ruleId, RuleItem::listFromRulesResponse(doc.object()));
    });
}

} // namespace smarthome::net
