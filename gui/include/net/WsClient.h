#ifndef SMARTHOME_GUI_NET_WSCLIENT_H
#define SMARTHOME_GUI_NET_WSCLIENT_H

#include "net/DtoModels.h"

#include <QAbstractSocket>
#include <QObject>
#include <QString>
#include <QUrl>

class QTimer;
class QWebSocket;

namespace smarthome::net {

/**
 * @brief WebSocket client for real-time telemetry streaming (/ws/telemetry).
 *
 * Handles initial snapshot and live flat DeviceTelemetrySchema frames,
 * keepalive "ping"/"pong" verification, and exponential-backoff reconnection.
 */
class WsClient : public QObject {
    Q_OBJECT

public:
    explicit WsClient(const QUrl &url = QUrl(QStringLiteral("ws://127.0.0.1:8001/ws/telemetry")),
                      QObject *parent = nullptr);
    ~WsClient() override;

    QUrl url() const { return url_; }
    void setUrl(const QUrl &url);

    QString deviceFilter() const { return deviceFilter_; }
    void setDeviceFilter(const QString &deviceName);

    bool autoReconnect() const { return autoReconnect_; }
    void setAutoReconnect(bool enabled);

    void setReconnectIntervals(int baseDelayMs, int maxDelayMs);
    void setPingIntervals(int pingIntervalMs, int pongTimeoutMs);

    bool isConnected() const;

    void connectToGateway();
    void connectToGateway(const QUrl &url, const QString &deviceFilter = QString());
    void disconnectFromGateway();
    void sendPing();

signals:
    void connected();
    void disconnected();
    void reconnecting(int attempt, int delayMs);
    void telemetryReceived(const smarthome::net::DeviceTelemetry &telemetry);
    void pongReceived();
    void protocolError(const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketTextMessageReceived(const QString &message);
    void onSocketErrorOccurred(QAbstractSocket::SocketError error);
    void onPingTimerTimeout();
    void onPongTimeout();
    void onReconnectTimerTimeout();

private:
    QUrl effectiveUrl() const;
    void openSocket();
    void scheduleReconnect();
    void stopKeepalive();

    QWebSocket *webSocket_;
    QTimer *pingTimer_;
    QTimer *pongTimeoutTimer_;
    QTimer *reconnectTimer_;

    QUrl url_;
    QString deviceFilter_;

    bool autoReconnect_ = true;
    bool manualDisconnect_ = false;
    bool awaitingPong_ = false;

    int reconnectAttempt_ = 0;
    int reconnectBaseDelayMs_ = 1000;
    int reconnectMaxDelayMs_ = 30000;
    int pingIntervalMs_ = 15000;
    int pongTimeoutMs_ = 5000;
};

} // namespace smarthome::net

#endif // SMARTHOME_GUI_NET_WSCLIENT_H
