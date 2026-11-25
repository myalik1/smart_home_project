#include "MainWindow.h"
#include "widgets/DashboardWidget.h"
#include "widgets/DeviceCardWidget.h"

#include <QHash>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>

#include <functional>

using namespace smarthome::net;
using namespace smarthome::ui;

namespace {

class TestGatewayServer : public QObject {
    Q_OBJECT
public:
    using HttpHandler = std::function<QByteArray(const QByteArray &target, int &statusCode)>;

    explicit TestGatewayServer(HttpHandler handler, QObject *parent = nullptr)
        : QObject(parent)
        , httpServer_(new QTcpServer(this))
        , wsServer_(new QWebSocketServer(QStringLiteral("TestWs"), QWebSocketServer::NonSecureMode, this))
        , handler_(std::move(handler)) {
        connect(httpServer_, &QTcpServer::newConnection, this, &TestGatewayServer::onHttpConnection);
        connect(wsServer_, &QWebSocketServer::newConnection, this, &TestGatewayServer::onWsConnection);
        httpServer_->listen(QHostAddress::LocalHost, 0);
        wsServer_->listen(QHostAddress::LocalHost, 0);
    }

    QUrl restUrl() const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1").arg(httpServer_->serverPort()));
    }

    QUrl wsUrl() const {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/ws/telemetry").arg(wsServer_->serverPort()));
    }

    void broadcastWsJson(const QString &jsonText) {
        for (QWebSocket *client : std::as_const(wsClients_)) {
            client->sendTextMessage(jsonText);
        }
    }

private slots:
    void onHttpConnection() {
        while (QTcpSocket *socket = httpServer_->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                QByteArray &buf = buffers_[socket];
                buf.append(socket->readAll());
                const int headerEnd = buf.indexOf("\r\n\r\n");
                if (headerEnd < 0) {
                    return;
                }
                const QList<QByteArray> requestLine = buf.left(headerEnd).split('\n').first().trimmed().split(' ');
                if (requestLine.size() < 2) {
                    return;
                }
                const QByteArray target = requestLine[1];
                buffers_.remove(socket);

                int code = 200;
                const QByteArray body = handler_(target, code);
                QByteArray raw;
                raw += "HTTP/1.1 " + QByteArray::number(code) + (code == 200 ? " OK\r\n" : " ERROR\r\n");
                raw += "Content-Type: application/json\r\n";
                raw += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
                raw += "Connection: close\r\n\r\n";
                raw += body;

                socket->write(raw);
                socket->flush();
                socket->disconnectFromHost();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            });
        }
    }

    void onWsConnection() {
        while (QWebSocket *client = wsServer_->nextPendingConnection()) {
            wsClients_.append(client);
            connect(client, &QWebSocket::disconnected, this, [this, client]() {
                wsClients_.removeAll(client);
                client->deleteLater();
            });
        }
    }

private:
    QTcpServer *httpServer_;
    QWebSocketServer *wsServer_;
    HttpHandler handler_;
    QHash<QTcpSocket *, QByteArray> buffers_;
    QVector<QWebSocket *> wsClients_;
};

DeviceStatus makeDeviceStatus(const QString &name,
                              const QString &type,
                              bool online,
                              const DeviceTelemetry &telemetry) {
    DeviceStatus status;
    status.deviceName = name;
    status.deviceType = type;
    status.topic = QStringLiteral("home/%1").arg(name.toLower());
    status.isOnline = online;
    status.lastSeenMs = telemetry.timestamp;
    status.telemetry = telemetry;
    return status;
}

} // namespace

class DashboardTest : public QObject {
    Q_OBJECT

private slots:
    void testDeviceCardsRenderProfileMetricsForAllFiveDevices() {
        DashboardWidget dashboard;

        DeviceTelemetry lamp;
        lamp.deviceName = QStringLiteral("Lamp1");
        lamp.deviceType = QStringLiteral("Lamp");
        lamp.timestampIso = QStringLiteral("2026-11-25T00:10:00.000+00:00");
        lamp.isOn = true;
        lamp.brightness = 80;

        DeviceTelemetry plug;
        plug.deviceName = QStringLiteral("Plug1");
        plug.deviceType = QStringLiteral("Plug");
        plug.timestamp = 1762605000000;
        plug.isOn = false;
        plug.currentPower = 12.5;

        DeviceTelemetry rad;
        rad.deviceName = QStringLiteral("Rad1");
        rad.deviceType = QStringLiteral("Radiator");
        rad.isOn = true;
        rad.currentValveLevel = 65;
        rad.currentTemperature = 23.2;

        DeviceTelemetry therm;
        therm.deviceName = QStringLiteral("Therm1");
        therm.deviceType = QStringLiteral("Thermostat");
        therm.currentTemperature = 21.4;
        therm.targetTemperature = 24.0;
        therm.mode = QStringLiteral("HEAT");

        DeviceTelemetry hygro;
        hygro.deviceName = QStringLiteral("Hygro1");
        hygro.deviceType = QStringLiteral("Hygrometer");
        hygro.currentHumidity = 48.5;
        hygro.currentTemperature = 22.1;

        dashboard.setDevices({
            makeDeviceStatus(QStringLiteral("Lamp1"), QStringLiteral("Lamp"), true, lamp),
            makeDeviceStatus(QStringLiteral("Plug1"), QStringLiteral("Plug"), false, plug),
            makeDeviceStatus(QStringLiteral("Rad1"), QStringLiteral("Radiator"), false, rad),
            makeDeviceStatus(QStringLiteral("Therm1"), QStringLiteral("Thermostat"), true, therm),
            makeDeviceStatus(QStringLiteral("Hygro1"), QStringLiteral("Hygrometer"), false, hygro),
        });

        QCOMPARE(dashboard.deviceCount(), 5);
        QCOMPARE(dashboard.onlineCount(), 2);

        auto *lampCard = dashboard.cardForDevice(QStringLiteral("Lamp1"));
        QVERIFY(lampCard != nullptr);
        QCOMPARE(lampCard->onlineBadgeText(), QStringLiteral("Online"));
        QVERIFY(lampCard->metricsText().contains(QStringLiteral("80 %")));
        QVERIFY(lampCard->metricsText().contains(QStringLiteral("ВКЛ")));

        auto *plugCard = dashboard.cardForDevice(QStringLiteral("Plug1"));
        QVERIFY(plugCard != nullptr);
        QCOMPARE(plugCard->onlineBadgeText(), QStringLiteral("Offline"));
        QVERIFY(plugCard->metricsText().contains(QStringLiteral("12.5 Вт")));

        auto *radCard = dashboard.cardForDevice(QStringLiteral("Rad1"));
        QVERIFY(radCard != nullptr);
        QVERIFY(radCard->metricsText().contains(QStringLiteral("65 %")));
        QVERIFY(radCard->metricsText().contains(QStringLiteral("23.2 °C")));

        auto *thermCard = dashboard.cardForDevice(QStringLiteral("Therm1"));
        QVERIFY(thermCard != nullptr);
        QVERIFY(thermCard->metricsText().contains(QStringLiteral("21.4 °C")));
        QVERIFY(thermCard->metricsText().contains(QStringLiteral("24.0 °C")));
        QVERIFY(thermCard->metricsText().contains(QStringLiteral("HEAT")));

        auto *hygroCard = dashboard.cardForDevice(QStringLiteral("Hygro1"));
        QVERIFY(hygroCard != nullptr);
        QVERIFY(hygroCard->metricsText().contains(QStringLiteral("48.5 %")));
        QVERIFY(hygroCard->metricsText().contains(QStringLiteral("22.1 °C")));

        // Live telemetry switches Offline -> Online and updates metrics
        DeviceTelemetry hygroLive = hygro;
        hygroLive.currentHumidity = 55.0;
        dashboard.updateTelemetry(hygroLive);

        QCOMPARE(hygroCard->onlineBadgeText(), QStringLiteral("Online"));
        QVERIFY(hygroCard->metricsText().contains(QStringLiteral("55.0 %")));
        QCOMPARE(dashboard.onlineCount(), 3);
    }

    void testMainWindowStartupRestWsAndHealthStatusBar() {
        bool mqttUp = true;
        bool failHealth = false;

        TestGatewayServer server([&](const QByteArray &target, int &statusCode) -> QByteArray {
            if (target == "/api/v1/devices") {
                statusCode = 200;
                return R"([
                    {"deviceName":"Lamp1","deviceType":"Lamp","topic":"home/lamp1","isOnline":false,"lastSeenMs":null,"telemetry":null},
                    {"deviceName":"Plug1","deviceType":"Plug","topic":"home/plug1","isOnline":false,"lastSeenMs":null,"telemetry":null},
                    {"deviceName":"Rad1","deviceType":"Radiator","topic":"home/radiator1","isOnline":false,"lastSeenMs":null,"telemetry":null},
                    {"deviceName":"Therm1","deviceType":"Thermostat","topic":"home/therm1","isOnline":false,"lastSeenMs":null,"telemetry":null},
                    {"deviceName":"Hygro1","deviceType":"Hygrometer","topic":"home/hygro1","isOnline":false,"lastSeenMs":null,"telemetry":null}
                ])";
            }
            if (target == "/api/v1/health") {
                if (failHealth) {
                    statusCode = 503;
                    return R"({"detail":"Gateway down"})";
                }
                statusCode = 200;
                return mqttUp
                    ? R"({"status":"ok","version":"1.0.0","mqtt_connected":true})"
                    : R"({"status":"ok","version":"1.0.0","mqtt_connected":false})";
            }
            statusCode = 404;
            return R"({"detail":"Not found"})";
        });

        MainWindow window(server.restUrl(), server.wsUrl(), 500);

        QTRY_COMPARE_WITH_TIMEOUT(window.dashboardWidget()->deviceCount(), 5, 2000);
        QCOMPARE(window.dashboardWidget()->onlineCount(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(window.gatewayStatusText().contains(QStringLiteral("Online")), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(window.mqttStatusText().contains(QStringLiteral("Подключён")), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(window.wsStatusText().contains(QStringLiteral("Подключён")), 2000);

        // Broadcast live telemetry over WebSocket -> Plug1 becomes Online with new power
        server.broadcastWsJson(QStringLiteral(
            R"({"deviceName":"Plug1","deviceType":"Plug","timestamp":1762605099000,"timestampIso":"2026-11-25T00:20:00.000+00:00","isOn":true,"currentPower":245.5})"));

        auto *plugCard = window.dashboardWidget()->cardForDevice(QStringLiteral("Plug1"));
        QVERIFY(plugCard != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(plugCard->onlineBadgeText(), QStringLiteral("Online"), 2000);
        QVERIFY(plugCard->metricsText().contains(QStringLiteral("245.5 Вт")));

        // Verify periodic health reflects MQTT disconnect and Gateway offline
        mqttUp = false;
        QTRY_VERIFY_WITH_TIMEOUT(window.mqttStatusText().contains(QStringLiteral("Отключён")), 2000);

        failHealth = true;
        QTRY_VERIFY_WITH_TIMEOUT(window.gatewayStatusText().contains(QStringLiteral("Offline")), 2000);
    }
};

QTEST_MAIN(DashboardTest)
#include "test_Dashboard.moc"
