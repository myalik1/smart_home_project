#include "net/WsClient.h"

#include <QTimer>
#include <QUrlQuery>
#include <QWebSocket>

namespace smarthome::net {

WsClient::WsClient(const QUrl &url, QObject *parent)
    : QObject(parent)
    , webSocket_(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , pingTimer_(new QTimer(this))
    , pongTimeoutTimer_(new QTimer(this))
    , reconnectTimer_(new QTimer(this))
    , url_(url) {
    registerDtoMetaTypes();

    pongTimeoutTimer_->setSingleShot(true);
    reconnectTimer_->setSingleShot(true);

    connect(webSocket_, &QWebSocket::connected, this, &WsClient::onSocketConnected);
    connect(webSocket_, &QWebSocket::disconnected, this, &WsClient::onSocketDisconnected);
    connect(webSocket_, &QWebSocket::textMessageReceived, this, &WsClient::onSocketTextMessageReceived);
    connect(webSocket_, &QWebSocket::errorOccurred, this, &WsClient::onSocketErrorOccurred);

    connect(pingTimer_, &QTimer::timeout, this, &WsClient::onPingTimerTimeout);
    connect(pongTimeoutTimer_, &QTimer::timeout, this, &WsClient::onPongTimeout);
    connect(reconnectTimer_, &QTimer::timeout, this, &WsClient::onReconnectTimerTimeout);
}

WsClient::~WsClient() {
    manualDisconnect_ = true;
    stopKeepalive();
    reconnectTimer_->stop();
}

void WsClient::setUrl(const QUrl &url) {
    url_ = url;
}

void WsClient::setDeviceFilter(const QString &deviceName) {
    deviceFilter_ = deviceName;
}

void WsClient::setAutoReconnect(bool enabled) {
    autoReconnect_ = enabled;
    if (!enabled) {
        reconnectTimer_->stop();
    }
}

void WsClient::setReconnectIntervals(int baseDelayMs, int maxDelayMs) {
    reconnectBaseDelayMs_ = qMax(50, baseDelayMs);
    reconnectMaxDelayMs_ = qMax(reconnectBaseDelayMs_, maxDelayMs);
}

void WsClient::setPingIntervals(int pingIntervalMs, int pongTimeoutMs) {
    pingIntervalMs_ = pingIntervalMs;
    pongTimeoutMs_ = qMax(50, pongTimeoutMs);
    if (isConnected()) {
        if (pingIntervalMs_ > 0) {
            pingTimer_->start(pingIntervalMs_);
        } else {
            pingTimer_->stop();
        }
    }
}

bool WsClient::isConnected() const {
    return webSocket_->state() == QAbstractSocket::ConnectedState;
}

QUrl WsClient::effectiveUrl() const {
    QUrl target = url_;
    if (!deviceFilter_.trimmed().isEmpty()) {
        QUrlQuery query(target);
        query.removeAllQueryItems(QStringLiteral("device"));
        query.addQueryItem(QStringLiteral("device"), deviceFilter_.trimmed());
        target.setQuery(query);
    }
    return target;
}

void WsClient::connectToGateway() {
    manualDisconnect_ = false;
    reconnectAttempt_ = 0;
    reconnectTimer_->stop();
    openSocket();
}

void WsClient::connectToGateway(const QUrl &url, const QString &deviceFilter) {
    url_ = url;
    deviceFilter_ = deviceFilter;
    connectToGateway();
}

void WsClient::disconnectFromGateway() {
    manualDisconnect_ = true;
    reconnectTimer_->stop();
    stopKeepalive();
    if (webSocket_->state() != QAbstractSocket::UnconnectedState) {
        webSocket_->close();
    }
}

void WsClient::openSocket() {
    stopKeepalive();
    if (webSocket_->state() != QAbstractSocket::UnconnectedState) {
        webSocket_->abort();
    }
    webSocket_->open(effectiveUrl());
}

void WsClient::sendPing() {
    if (!isConnected()) {
        return;
    }
    webSocket_->sendTextMessage(QStringLiteral("ping"));
    if (!awaitingPong_) {
        awaitingPong_ = true;
        pongTimeoutTimer_->start(pongTimeoutMs_);
    }
}

void WsClient::onSocketConnected() {
    reconnectAttempt_ = 0;
    reconnectTimer_->stop();
    awaitingPong_ = false;
    pongTimeoutTimer_->stop();

    if (pingIntervalMs_ > 0) {
        pingTimer_->start(pingIntervalMs_);
    }
    emit connected();
}

void WsClient::onSocketDisconnected() {
    stopKeepalive();
    emit disconnected();
    if (!manualDisconnect_ && autoReconnect_) {
        scheduleReconnect();
    }
}

void WsClient::onSocketTextMessageReceived(const QString &message) {
    const QString trimmed = message.trimmed();
    if (trimmed.compare(QStringLiteral("pong"), Qt::CaseInsensitive) == 0) {
        awaitingPong_ = false;
        pongTimeoutTimer_->stop();
        emit pongReceived();
        return;
    }

    QString errorMessage;
    const auto telemetry = DeviceTelemetry::fromJsonBytes(trimmed.toUtf8(), &errorMessage);
    if (!telemetry.has_value()) {
        emit protocolError(errorMessage);
        return;
    }

    emit telemetryReceived(*telemetry);
}

void WsClient::onSocketErrorOccurred(QAbstractSocket::SocketError /*error*/) {
    emit errorOccurred(webSocket_->errorString());
    if (!manualDisconnect_ && autoReconnect_
        && webSocket_->state() == QAbstractSocket::UnconnectedState
        && !reconnectTimer_->isActive()) {
        scheduleReconnect();
    }
}

void WsClient::onPingTimerTimeout() {
    sendPing();
}

void WsClient::onPongTimeout() {
    awaitingPong_ = false;
    pingTimer_->stop();
    emit errorOccurred(QStringLiteral("WebSocket keepalive timeout: 'pong' not received"));
    webSocket_->abort();
}

void WsClient::onReconnectTimerTimeout() {
    if (!manualDisconnect_ && autoReconnect_) {
        openSocket();
    }
}

void WsClient::scheduleReconnect() {
    if (reconnectTimer_->isActive()) {
        return;
    }
    const int shift = qMin(reconnectAttempt_, 5);
    const int delayMs = qMin(reconnectBaseDelayMs_ * (1 << shift), reconnectMaxDelayMs_);
    ++reconnectAttempt_;
    reconnectTimer_->start(delayMs);
    emit reconnecting(reconnectAttempt_, delayMs);
}

void WsClient::stopKeepalive() {
    pingTimer_->stop();
    pongTimeoutTimer_->stop();
    awaitingPong_ = false;
}

} // namespace smarthome::net
