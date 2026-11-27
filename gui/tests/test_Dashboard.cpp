#include "MainWindow.h"
#include "net/WsClient.h"
#include "widgets/AddRuleDialog.h"
#include "widgets/AnalyticsWidget.h"
#include "widgets/DashboardWidget.h"
#include "widgets/DeviceCardWidget.h"
#include "widgets/RulesAndSettingsWidget.h"

#include <QApplication>
#include <QChart>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineSeries>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

#include <functional>

using namespace smarthome::net;
using namespace smarthome::ui;

namespace {

class TestGatewayServer : public QObject {
    Q_OBJECT
public:
    using HttpHandler = std::function<QByteArray(const QByteArray &method,
                                                 const QByteArray &target,
                                                 const QByteArray &body,
                                                 int &statusCode)>;

    explicit TestGatewayServer(HttpHandler handler,
                               quint16 httpPort = 0,
                               quint16 wsPort = 0,
                               bool autoStart = true,
                               QObject *parent = nullptr)
        : QObject(parent)
        , httpServer_(new QTcpServer(this))
        , wsServer_(new QWebSocketServer(QStringLiteral("TestWs"), QWebSocketServer::NonSecureMode, this))
        , handler_(std::move(handler)) {
        connect(httpServer_, &QTcpServer::newConnection, this, &TestGatewayServer::onHttpConnection);
        connect(wsServer_, &QWebSocketServer::newConnection, this, &TestGatewayServer::onWsConnection);
        if (autoStart) {
            start(httpPort, wsPort);
        }
    }

    bool start(quint16 httpPort = 0, quint16 wsPort = 0) {
        const bool httpOk = httpServer_->isListening() || httpServer_->listen(QHostAddress::LocalHost, httpPort);
        const bool wsOk = wsServer_->isListening() || wsServer_->listen(QHostAddress::LocalHost, wsPort);
        return httpOk && wsOk;
    }

    void stop() {
        httpServer_->close();
        wsServer_->close();
        const auto clientsCopy = wsClients_;
        for (QWebSocket *client : clientsCopy) {
            client->abort();
        }
    }

    quint16 httpPort() const { return httpServer_->serverPort(); }
    quint16 wsPort() const { return wsServer_->serverPort(); }

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
                const QByteArray headerBlock = buf.left(headerEnd);
                const QList<QByteArray> lines = headerBlock.split('\n');
                const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
                if (requestLine.size() < 2) {
                    return;
                }
                int contentLength = 0;
                for (int i = 1; i < lines.size(); ++i) {
                    const QByteArray line = lines[i].trimmed();
                    if (line.toLower().startsWith("content-length:")) {
                        contentLength = line.mid(15).trimmed().toInt();
                    }
                }
                if (buf.size() < headerEnd + 4 + contentLength) {
                    return;
                }

                const QByteArray method = requestLine[0];
                const QByteArray target = requestLine[1];
                const QByteArray reqBody = buf.mid(headerEnd + 4, contentLength);
                buffers_.remove(socket);

                int code = 200;
                const QByteArray body = handler_(method, target, reqBody, code);
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

        TestGatewayServer server([&](const QByteArray &,
                                     const QByteArray &target,
                                     const QByteArray &,
                                     int &statusCode) -> QByteArray {
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

    void testInteractiveDeviceControlsAndAntiSpam() {
        qRegisterMetaType<CommandRequest>("smarthome::net::CommandRequest");

        DashboardWidget dashboard;
        DeviceTelemetry lamp;
        lamp.deviceName = QStringLiteral("Lamp1");
        lamp.deviceType = QStringLiteral("Lamp");
        lamp.brightness = 20;

        DeviceTelemetry rad;
        rad.deviceName = QStringLiteral("Rad1");
        rad.deviceType = QStringLiteral("Radiator");
        rad.currentValveLevel = 10;

        DeviceTelemetry therm;
        therm.deviceName = QStringLiteral("Therm1");
        therm.deviceType = QStringLiteral("Thermostat");
        therm.targetTemperature = 21.0;
        therm.mode = QStringLiteral("OFF");

        DeviceTelemetry plug;
        plug.deviceName = QStringLiteral("Plug1");
        plug.deviceType = QStringLiteral("Plug");

        DeviceTelemetry hygro;
        hygro.deviceName = QStringLiteral("Hygro1");
        hygro.deviceType = QStringLiteral("Hygrometer");

        dashboard.setDevices({
            makeDeviceStatus(QStringLiteral("Lamp1"), QStringLiteral("Lamp"), true, lamp),
            makeDeviceStatus(QStringLiteral("Plug1"), QStringLiteral("Plug"), true, plug),
            makeDeviceStatus(QStringLiteral("Rad1"), QStringLiteral("Radiator"), true, rad),
            makeDeviceStatus(QStringLiteral("Therm1"), QStringLiteral("Thermostat"), true, therm),
            makeDeviceStatus(QStringLiteral("Hygro1"), QStringLiteral("Hygrometer"), true, hygro),
        });

        auto *lampCard = dashboard.cardForDevice(QStringLiteral("Lamp1"));
        auto *plugCard = dashboard.cardForDevice(QStringLiteral("Plug1"));
        auto *radCard = dashboard.cardForDevice(QStringLiteral("Rad1"));
        auto *thermCard = dashboard.cardForDevice(QStringLiteral("Therm1"));
        auto *hygroCard = dashboard.cardForDevice(QStringLiteral("Hygro1"));

        QVERIFY(lampCard->onButton() != nullptr && lampCard->offButton() != nullptr);
        QVERIFY(plugCard->onButton() != nullptr && plugCard->offButton() != nullptr);
        QVERIFY(radCard->onButton() != nullptr && radCard->offButton() != nullptr);
        QVERIFY(thermCard->onButton() != nullptr && thermCard->offButton() != nullptr);
        QVERIFY(hygroCard->onButton() == nullptr && hygroCard->offButton() == nullptr);

        QSignalSpy cmdSpy(&dashboard, &DashboardWidget::commandRequested);

        // Power buttons
        lampCard->onButton()->click();
        plugCard->offButton()->click();
        QCOMPARE(cmdSpy.count(), 2);
        QCOMPARE(cmdSpy.at(0).at(0).toString(), QStringLiteral("Lamp1"));
        QCOMPARE(cmdSpy.at(0).at(1).value<CommandRequest>().command, QStringLiteral("ON"));
        QCOMPARE(cmdSpy.at(1).at(0).toString(), QStringLiteral("Plug1"));
        QCOMPARE(cmdSpy.at(1).at(1).value<CommandRequest>().command, QStringLiteral("OFF"));
        cmdSpy.clear();

        // Slider drag anti-spam: dragging emits 0 commands until sliderReleased()
        lampCard->setDebounceIntervalMs(50);
        lampCard->levelSlider()->setSliderDown(true);
        lampCard->levelSlider()->setValue(35);
        lampCard->levelSlider()->setValue(55);
        lampCard->levelSlider()->setValue(75);
        QTest::qWait(80);
        QCOMPARE(cmdSpy.count(), 0);

        // QAbstractSlider::setSliderDown(false) emits sliderReleased() automatically
        lampCard->levelSlider()->setSliderDown(false);
        QCOMPARE(cmdSpy.count(), 1);
        QCOMPARE(cmdSpy.first().at(0).toString(), QStringLiteral("Lamp1"));
        QCOMPARE(cmdSpy.first().at(1).value<CommandRequest>().command, QStringLiteral("SET_BRIGHTNESS"));
        QCOMPARE(*cmdSpy.first().at(1).value<CommandRequest>().value, 75.0);
        cmdSpy.clear();

        // Slider click/step debounce: rapid updates coalesce into 1 SET_VALVE_LEVEL
        radCard->setDebounceIntervalMs(50);
        radCard->levelSlider()->setValue(30);
        radCard->levelSlider()->setValue(60);
        radCard->levelSlider()->setValue(90);
        QTRY_COMPARE_WITH_TIMEOUT(cmdSpy.count(), 1, 500);
        QCOMPARE(cmdSpy.first().at(0).toString(), QStringLiteral("Rad1"));
        QCOMPARE(cmdSpy.first().at(1).value<CommandRequest>().command, QStringLiteral("SET_VALVE_LEVEL"));
        QCOMPARE(*cmdSpy.first().at(1).value<CommandRequest>().value, 90.0);
        cmdSpy.clear();

        // Thermostat target temp spinbox (debounced SET_TEMPERATURE) and mode combobox (SET_MODE)
        thermCard->setDebounceIntervalMs(50);
        thermCard->targetTempSpinBox()->setValue(23.5);
        thermCard->targetTempSpinBox()->setValue(24.5);
        QTRY_COMPARE_WITH_TIMEOUT(cmdSpy.count(), 1, 500);
        QCOMPARE(cmdSpy.first().at(0).toString(), QStringLiteral("Therm1"));
        QCOMPARE(cmdSpy.first().at(1).value<CommandRequest>().command, QStringLiteral("SET_TEMPERATURE"));
        QCOMPARE(*cmdSpy.first().at(1).value<CommandRequest>().value, 24.5);
        cmdSpy.clear();

        thermCard->modeComboBox()->setCurrentText(QStringLiteral("HEAT"));
        QCOMPARE(cmdSpy.count(), 1);
        QCOMPARE(cmdSpy.first().at(0).toString(), QStringLiteral("Therm1"));
        QCOMPARE(cmdSpy.first().at(1).value<CommandRequest>().command, QStringLiteral("SET_MODE"));
        QCOMPARE(*cmdSpy.first().at(1).value<CommandRequest>().mode, QStringLiteral("HEAT"));
        cmdSpy.clear();

        // Thermostat ON/OFF buttons map to SET_MODE HEAT / OFF and activePower tracks mode
        thermCard->offButton()->click();
        thermCard->onButton()->click();
        QCOMPARE(cmdSpy.count(), 2);
        QCOMPARE(cmdSpy.at(0).at(1).value<CommandRequest>().command, QStringLiteral("SET_MODE"));
        QCOMPARE(*cmdSpy.at(0).at(1).value<CommandRequest>().mode, QStringLiteral("OFF"));
        QCOMPARE(cmdSpy.at(1).at(1).value<CommandRequest>().command, QStringLiteral("SET_MODE"));
        QCOMPARE(*cmdSpy.at(1).at(1).value<CommandRequest>().mode, QStringLiteral("HEAT"));

        DeviceTelemetry thermHeat = therm;
        thermHeat.mode = QStringLiteral("HEAT");
        thermHeat.isOn = false;
        thermCard->applyLiveTelemetry(thermHeat);
        QCOMPARE(thermCard->onButton()->property("activePower").toBool(), true);
        QCOMPARE(thermCard->offButton()->property("activePower").toBool(), false);
    }

    void testMainWindowSendsCommandsAndHandles503() {
        bool brokerAvailable = true;
        QByteArray lastCommandBody;

        TestGatewayServer server([&](const QByteArray &method,
                                     const QByteArray &target,
                                     const QByteArray &body,
                                     int &statusCode) -> QByteArray {
            if (target == "/api/v1/devices") {
                statusCode = 200;
                return R"([{"deviceName":"Lamp1","deviceType":"Lamp","topic":"home/lamp1","isOnline":true,"lastSeenMs":1762605000000,"telemetry":{"deviceName":"Lamp1","deviceType":"Lamp","isOn":false,"brightness":50}}])";
            }
            if (target == "/api/v1/health") {
                statusCode = 200;
                return brokerAvailable
                    ? R"({"status":"ok","version":"1.0.0","mqtt_connected":true})"
                    : R"({"status":"ok","version":"1.0.0","mqtt_connected":false})";
            }
            if (method == "POST" && target == "/api/v1/devices/Lamp1/command") {
                lastCommandBody = body;
                if (!brokerAvailable) {
                    statusCode = 503;
                    return R"({"detail":"MQTT broker is currently unavailable. Command cannot be delivered."})";
                }
                statusCode = 200;
                return R"({"status":"sent","deviceName":"Lamp1","topic":"home/lamp1/set","payload":{"command":"ON"}})";
            }
            statusCode = 404;
            return R"({"detail":"Not found"})";
        });

        MainWindow window(server.restUrl(), server.wsUrl(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(window.dashboardWidget()->deviceCount(), 1, 2000);

        auto *lampCard = window.dashboardWidget()->cardForDevice(QStringLiteral("Lamp1"));
        QVERIFY(lampCard != nullptr);
        lampCard->onButton()->click();

        QTRY_VERIFY_WITH_TIMEOUT(!lastCommandBody.isEmpty(), 2000);
        QCOMPARE(QJsonDocument::fromJson(lastCommandBody).object().value(QStringLiteral("command")).toString(),
                 QStringLiteral("ON"));
        QVERIFY(!window.isAlertBannerVisible());

        // Now simulate 503 Service Unavailable from MQTT broker outage
        brokerAvailable = false;
        lampCard->offButton()->click();

        QTRY_VERIFY_WITH_TIMEOUT(window.isAlertBannerVisible(), 2000);
        QVERIFY(window.alertBannerText().contains(QStringLiteral("Нет связи с MQTT-брокером")));
    }

    void testAnalyticsTabFiltersChartsAndEventsTable() {
        QByteArray lastHistoryQuery;
        QByteArray lastAllHistoryTarget;

        TestGatewayServer server([&](const QByteArray &method,
                                     const QByteArray &target,
                                     const QByteArray &,
                                     int &statusCode) -> QByteArray {
            if (method == "GET" && target == "/api/v1/devices") {
                statusCode = 200;
                return R"([
                    {"deviceName":"Therm1","deviceType":"Thermostat","topic":"home/therm1","isOnline":true},
                    {"deviceName":"Hygro1","deviceType":"Hygrometer","topic":"home/hygro1","isOnline":true}
                ])";
            }
            if (method == "GET" && target == "/api/v1/health") {
                statusCode = 200;
                return R"({"status":"ok","version":"1.0.0","mqtt_connected":true})";
            }
            if (method == "GET" && target.startsWith("/api/v1/devices/Therm1/history")) {
                lastHistoryQuery = target;
                statusCode = 200;
                return R"([
                    {"deviceName":"Therm1","deviceType":"Thermostat","timestamp":1762605000000,"isOn":true,"currentTemperature":21.0,"targetTemperature":23.5,"mode":"HEAT"},
                    {"deviceName":"Therm1","deviceType":"Thermostat","timestamp":1762605060000,"isOn":true,"currentTemperature":22.2,"targetTemperature":23.5,"mode":"HEAT"}
                ])";
            }
            if (method == "GET" && target.startsWith("/api/v1/devices/Hygro1/history")) {
                lastHistoryQuery = target;
                statusCode = 200;
                return R"([
                    {"deviceName":"Hygro1","deviceType":"Hygrometer","timestamp":1762605120000,"isOn":true,"currentHumidity":49.5,"currentTemperature":22.0}
                ])";
            }
            if (method == "GET" && target.startsWith("/api/v1/history")) {
                lastAllHistoryTarget = target;
                statusCode = 200;
                return R"([
                    {"deviceName":"Hygro1","deviceType":"Hygrometer","timestamp":1762605120000,"timestampIso":"2026-11-26T07:00:00.000+00:00","isOn":true,"currentHumidity":49.5,"currentTemperature":22.0},
                    {"deviceName":"Therm1","deviceType":"Thermostat","timestamp":1762605060000,"timestampIso":"2026-11-26T06:59:00.000+00:00","isOn":true,"currentTemperature":22.2,"targetTemperature":23.5,"mode":"HEAT"}
                ])";
            }
            statusCode = 404;
            return R"({"detail":"Not found"})";
        });

        MainWindow window(server.restUrl(), server.wsUrl(), 5000);
        auto *analytics = window.analyticsWidget();
        QVERIFY(analytics != nullptr);

        // Initial load populates Therm1 chart (2 series: current + target) and events table
        QTRY_VERIFY_WITH_TIMEOUT(analytics->chart()->series().size() == 2, 2000);
        auto *primarySeries = qobject_cast<QLineSeries *>(analytics->chart()->series().first());
        QVERIFY(primarySeries != nullptr);
        QCOMPARE(primarySeries->count(), 2);
        QVERIFY(lastHistoryQuery.contains("limit=1000"));
        QVERIFY(lastHistoryQuery.contains("start_ts="));
        QVERIFY(lastHistoryQuery.contains("end_ts="));

        QTRY_COMPARE_WITH_TIMEOUT(analytics->eventsTable()->rowCount(), 2, 2000);
        QCOMPARE(lastAllHistoryTarget, QByteArray("/api/v1/history?limit=100"));
        QCOMPARE(analytics->eventsTable()->item(0, 1)->text(), QStringLiteral("Hygro1"));

        // Manual date edits switch preset to custom (-1) and convert to Unix ms on refresh
        const QDateTime customStart = QDateTime::fromMSecsSinceEpoch(1762600000000);
        const QDateTime customEnd = QDateTime::fromMSecsSinceEpoch(1762606000000);
        analytics->startDateTimeEdit()->setDateTime(customStart);
        analytics->endDateTimeEdit()->setDateTime(customEnd);
        QCOMPARE(analytics->presetComboBox()->currentData().toInt(), -1);

        analytics->deviceComboBox()->setCurrentText(QStringLiteral("Hygro1"));
        QTRY_VERIFY_WITH_TIMEOUT(lastHistoryQuery.startsWith("/api/v1/devices/Hygro1/history"), 2000);
        QVERIFY(lastHistoryQuery.contains("start_ts=1762600000000"));
        QVERIFY(lastHistoryQuery.contains("end_ts=1762606000000"));
        QTRY_VERIFY_WITH_TIMEOUT(analytics->chart()->title().contains(QStringLiteral("Hygro1")), 2000);
    }

    void testAnalyticsLiveWebSocketFallbackWhenSqliteEmpty() {
        TestGatewayServer server([&](const QByteArray &method,
                                     const QByteArray &target,
                                     const QByteArray &,
                                     int &statusCode) -> QByteArray {
            if (method == "GET" && target == "/api/v1/devices") {
                statusCode = 200;
                return R"([{"deviceName":"Therm1","deviceType":"Thermostat","topic":"home/therm1","isOnline":true}])";
            }
            if (method == "GET" && target == "/api/v1/health") {
                statusCode = 200;
                return R"({"status":"ok","version":"1.0.0","mqtt_connected":true})";
            }
            if (method == "GET" && target.contains("/history")) {
                statusCode = 200;
                return "[]";
            }
            statusCode = 404;
            return R"({"detail":"Not found"})";
        });

        MainWindow window(server.restUrl(), server.wsUrl(), 5000);
        auto *analytics = window.analyticsWidget();
        QVERIFY(analytics != nullptr);

        QTRY_VERIFY_WITH_TIMEOUT(window.wsStatusText().contains(QStringLiteral("Подключён")), 2000);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        server.broadcastWsJson(QStringLiteral(
            R"({"deviceName":"Therm1","deviceType":"Thermostat","timestamp":%1,"isOn":true,"currentTemperature":20.5,"targetTemperature":23.0,"mode":"HEAT"})")
            .arg(nowMs - 2000));
        server.broadcastWsJson(QStringLiteral(
            R"({"deviceName":"Therm1","deviceType":"Thermostat","timestamp":%1,"isOn":true,"currentTemperature":21.2,"targetTemperature":23.0,"mode":"HEAT"})")
            .arg(nowMs));

        QTRY_VERIFY_WITH_TIMEOUT(analytics->chart()->series().size() == 2, 2000);
        auto *primarySeries = qobject_cast<QLineSeries *>(analytics->chart()->series().first());
        QVERIFY(primarySeries != nullptr);
        QCOMPARE(primarySeries->count(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(analytics->eventsTable()->rowCount(), 2, 2000);
    }

    void testRulesCrud409ConflictDiagnosticsAndUrlSwitch() {
        QJsonArray rulesArray = QJsonArray{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("night_lamp_off")},
                {QStringLiteral("type"), QStringLiteral("point")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("time"), QStringLiteral("23:30:00")},
                {QStringLiteral("repeatDaily"), true},
                {QStringLiteral("action"), QJsonObject{
                    {QStringLiteral("device"), QStringLiteral("Lamp1")},
                    {QStringLiteral("command"), QJsonObject{{QStringLiteral("command"), QStringLiteral("OFF")}}}
                }}
            }
        };

        auto makeRulesDoc = [&]() -> QByteArray {
            return QJsonDocument(QJsonObject{{QStringLiteral("rules"), rulesArray}}).toJson(QJsonDocument::Compact);
        };

        TestGatewayServer server1([&](const QByteArray &method,
                                      const QByteArray &target,
                                      const QByteArray &body,
                                      int &statusCode) -> QByteArray {
            if (method == "GET" && target == "/api/v1/devices") {
                statusCode = 200;
                return R"([{"deviceName":"Lamp1","deviceType":"Lamp","topic":"home/lamp1","isOnline":true}])";
            }
            if (method == "GET" && target == "/api/v1/health") {
                statusCode = 200;
                return R"({"status":"ok","version":"1.0.0","mqtt_broker":"localhost:1883","mqtt_connected":true,"sqlite_db_path":"data/smart_home.db","sqlite_db_exists":true,"telemetry_records_count":321,"rules_config_path":"data/rules.json","registered_devices_count":5,"online_devices":["Lamp1"],"active_ws_clients":1})";
            }
            if (method == "GET" && target.contains("/history")) {
                statusCode = 200;
                return "[]";
            }
            if (method == "GET" && target == "/api/v1/rules") {
                statusCode = 200;
                return makeRulesDoc();
            }
            if (method == "POST" && target == "/api/v1/rules/items") {
                const QJsonObject newRule = QJsonDocument::fromJson(body).object();
                const QString newId = newRule.value(QStringLiteral("id")).toString();
                for (const QJsonValue &existing : rulesArray) {
                    if (existing.toObject().value(QStringLiteral("id")).toString() == newId) {
                        statusCode = 409;
                        return QStringLiteral(R"({"detail":"Rule with id '%1' already exists."})").arg(newId).toUtf8();
                    }
                }
                rulesArray.append(newRule);
                statusCode = 201;
                return QJsonDocument(newRule).toJson(QJsonDocument::Compact);
            }
            if (method == "PATCH" && target.startsWith("/api/v1/rules/items/")) {
                const QString id = QString::fromUtf8(target.mid(20));
                const bool enabled = QJsonDocument::fromJson(body).object().value(QStringLiteral("enabled")).toBool();
                for (int i = 0; i < rulesArray.size(); ++i) {
                    QJsonObject obj = rulesArray.at(i).toObject();
                    if (obj.value(QStringLiteral("id")).toString() == id) {
                        obj.insert(QStringLiteral("enabled"), enabled);
                        rulesArray[i] = obj;
                        statusCode = 200;
                        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
                    }
                }
                statusCode = 404;
                return R"({"detail":"Not found"})";
            }
            if (method == "DELETE" && target.startsWith("/api/v1/rules/items/")) {
                const QString id = QString::fromUtf8(target.mid(20));
                for (int i = 0; i < rulesArray.size(); ++i) {
                    if (rulesArray.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
                        rulesArray.removeAt(i);
                        statusCode = 200;
                        return makeRulesDoc();
                    }
                }
                statusCode = 404;
                return R"({"detail":"Not found"})";
            }
            statusCode = 404;
            return R"({"detail":"Not found"})";
        });

        MainWindow window(server1.restUrl(), server1.wsUrl(), 5000);
        auto *rulesTab = window.rulesAndSettingsWidget();
        QVERIFY(rulesTab != nullptr);

        // 1. Initial GET /rules + GET /health diagnostics
        QTRY_COMPARE_WITH_TIMEOUT(rulesTab->rulesTable()->rowCount(), 1, 2000);
        QCOMPARE(rulesTab->rulesTable()->item(0, 0)->text(), QStringLiteral("night_lamp_off"));
        QVERIFY(rulesTab->diagDbPathText().contains(QStringLiteral("data/smart_home.db")));
        QCOMPARE(rulesTab->diagRecordsCountText(), QStringLiteral("321"));
        QVERIFY(rulesTab->diagMqttText().contains(QStringLiteral("Подключён")));

        // 2. Toggle enabled checkbox -> PATCH /api/v1/rules/items/{rule_id}
        QCheckBox *check = rulesTab->rowEnabledCheckBox(0);
        QVERIFY(check != nullptr);
        QCOMPARE(check->isChecked(), true);
        check->click();
        QTRY_COMPARE_WITH_TIMEOUT(rulesArray.at(0).toObject().value(QStringLiteral("enabled")).toBool(), false, 2000);

        // 3. AddRuleDialog: test 409 Conflict then valid creation
        bool dialogVerified = false;
        QTimer::singleShot(30, &window, [&]() {
            auto *dlg = rulesTab->activeAddRuleDialog();
            QVERIFY(dlg != nullptr);
            dlg->idEdit()->setText(QStringLiteral("night_lamp_off"));
            dlg->typeCombo()->setCurrentText(QStringLiteral("periodic"));
            dlg->createButton()->click();

            QTRY_VERIFY_WITH_TIMEOUT(dlg->errorLabel()->isVisible(), 2000);
            QVERIFY(dlg->errorLabel()->text().contains(QStringLiteral("409")));
            QVERIFY(dlg->isVisible());

            dlg->idEdit()->setText(QStringLiteral("periodic_lamp_ping"));
            dlg->createButton()->click();
            dialogVerified = true;
        });
        rulesTab->addRuleButton()->click();
        QVERIFY(dialogVerified);
        QTRY_COMPARE_WITH_TIMEOUT(rulesTab->rulesTable()->rowCount(), 2, 2000);
        QCOMPARE(rulesTab->rulesTable()->item(1, 0)->text(), QStringLiteral("periodic_lamp_ping"));

        // 4. Delete rule with QMessageBox confirmation -> DELETE /api/v1/rules/items/{rule_id}
        QPushButton *delBtn = rulesTab->rowDeleteButton(1);
        QVERIFY(delBtn != nullptr);
        QTimer::singleShot(30, &window, [&]() {
            if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                box->button(QMessageBox::Yes)->click();
            }
        });
        delBtn->click();
        QTRY_COMPARE_WITH_TIMEOUT(rulesTab->rulesTable()->rowCount(), 1, 2000);

        // 5. Switch gateway URL to a second server
        TestGatewayServer server2([&](const QByteArray &method,
                                      const QByteArray &target,
                                      const QByteArray &,
                                      int &statusCode) -> QByteArray {
            if (method == "GET" && target == "/api/v1/devices") {
                statusCode = 200;
                return "[]";
            }
            if (method == "GET" && target == "/api/v1/health") {
                statusCode = 200;
                return R"({"status":"ok","version":"2.0.0","mqtt_broker":"remote:1883","mqtt_connected":true,"sqlite_db_path":"remote.db","sqlite_db_exists":true,"telemetry_records_count":999,"rules_config_path":"remote_rules.json","registered_devices_count":0,"online_devices":[],"active_ws_clients":1})";
            }
            if (method == "GET" && target == "/api/v1/rules") {
                statusCode = 200;
                return R"({"rules":[]})";
            }
            if (method == "GET" && target.contains("/history")) {
                statusCode = 200;
                return "[]";
            }
            statusCode = 404;
            return R"({"detail":"Not found"})";
        });

        QUrl origin2 = server2.restUrl();
        origin2.setPath(QString());
        rulesTab->gatewayUrlEdit()->setText(origin2.toString());
        rulesTab->applyGatewayUrlButton()->click();

        QTRY_COMPARE_WITH_TIMEOUT(rulesTab->diagRecordsCountText(), QStringLiteral("999"), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(rulesTab->rulesTable()->rowCount(), 0, 2000);
        QVERIFY(window.gatewayStatusText().contains(QStringLiteral("v2.0.0")));
    }

    void testResilienceStartupOfflineGatewayRestartAndMosquitto503() {
        bool mqttConnected = true;

        // Reserve two free local ports, then close immediately so GUI starts while gateway is offline
        TestGatewayServer server([&](const QByteArray &method,
                                     const QByteArray &target,
                                     const QByteArray &,
                                     int &statusCode) -> QByteArray {
            if (method == "GET" && target == "/api/v1/devices") {
                statusCode = 200;
                return R"([{"deviceName":"Lamp1","deviceType":"Lamp","topic":"home/lamp1","isOnline":true,"lastSeenMs":1762605000000,"telemetry":{"deviceName":"Lamp1","deviceType":"Lamp","isOn":false,"brightness":40}}])";
            }
            if (method == "GET" && target == "/api/v1/health") {
                statusCode = 200;
                return mqttConnected
                    ? R"({"status":"ok","version":"1.0.0","mqtt_broker":"localhost:1883","mqtt_connected":true,"sqlite_db_path":"data/smart_home.db","sqlite_db_exists":true,"telemetry_records_count":5,"rules_config_path":"data/rules.json","registered_devices_count":1,"online_devices":["Lamp1"],"active_ws_clients":1})"
                    : R"({"status":"ok","version":"1.0.0","mqtt_broker":"localhost:1883","mqtt_connected":false,"sqlite_db_path":"data/smart_home.db","sqlite_db_exists":true,"telemetry_records_count":5,"rules_config_path":"data/rules.json","registered_devices_count":1,"online_devices":[],"active_ws_clients":1})";
            }
            if (method == "GET" && target == "/api/v1/rules") {
                statusCode = 200;
                return R"({"rules":[{"id":"r_auto","type":"point","enabled":true,"time":"20:00:00","action":{"device":"Lamp1","command":{"command":"ON"}}}]})";
            }
            if (method == "GET" && target.contains("/history")) {
                statusCode = 200;
                return "[]";
            }
            if (method == "POST" && target == "/api/v1/devices/Lamp1/command") {
                if (!mqttConnected) {
                    statusCode = 503;
                    return R"({"detail":"MQTT broker is currently unavailable. Command cannot be delivered."})";
                }
                statusCode = 200;
                return R"({"status":"sent","deviceName":"Lamp1","topic":"home/lamp1/set","payload":{"command":"ON"}})";
            }
            statusCode = 404;
            return R"({"detail":"Not found"})";
        });

        const quint16 httpPort = server.httpPort();
        const quint16 wsPort = server.wsPort();
        const QUrl restUrl = server.restUrl();
        const QUrl wsUrl = server.wsUrl();
        server.stop();

        // 1. Start GUI while gateway is OFF -> status Offline, 0 devices, no crash
        MainWindow window(restUrl, wsUrl, 500);
        window.wsClient()->setReconnectIntervals(60, 150);

        QTRY_VERIFY_WITH_TIMEOUT(window.gatewayStatusText().contains(QStringLiteral("Offline")), 5000);
        QCOMPARE(window.dashboardWidget()->deviceCount(), 0);

        // 2. Start gateway later -> health poll & WS reconnect auto-recover devices, rules, and WS
        QVERIFY(server.start(httpPort, wsPort));
        QTRY_VERIFY_WITH_TIMEOUT(window.gatewayStatusText().contains(QStringLiteral("Online")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(window.wsStatusText().contains(QStringLiteral("Подключён")), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(window.dashboardWidget()->deviceCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(window.rulesAndSettingsWidget()->rulesCount(), 1, 5000);

        // 3. Gateway restarts mid-session -> WS drops then reconnects automatically
        server.stop();
        QTRY_VERIFY_WITH_TIMEOUT(window.gatewayStatusText().contains(QStringLiteral("Offline")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!window.wsStatusText().contains(QStringLiteral("Подключён")), 5000);

        mqttConnected = false;
        QVERIFY(server.start(httpPort, wsPort));
        QTRY_VERIFY_WITH_TIMEOUT(window.gatewayStatusText().contains(QStringLiteral("Online")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(window.wsStatusText().contains(QStringLiteral("Подключён")), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(window.mqttStatusText().contains(QStringLiteral("Отключён")), 5000);

        // 4. Mosquitto disconnected while sending command -> 503 alert banner appears
        auto *lampCard = window.dashboardWidget()->cardForDevice(QStringLiteral("Lamp1"));
        QVERIFY(lampCard != nullptr);
        lampCard->onButton()->click();
        QTRY_VERIFY_WITH_TIMEOUT(window.isAlertBannerVisible(), 3000);
        QVERIFY(window.alertBannerText().contains(QStringLiteral("Нет связи с MQTT-брокером")));
    }
};

QTEST_MAIN(DashboardTest)
#include "test_Dashboard.moc"
