#include "net/ApiClient.h"
#include "net/DtoModels.h"
#include "net/WsClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

#include <functional>

using namespace smarthome::net;

namespace {

struct HttpRequest {
    QByteArray method;
    QByteArray target;
    QByteArray body;
};

struct HttpResponse {
    int statusCode = 200;
    QByteArray reason = "OK";
    QByteArray body;
};

class MiniHttpServer : public QObject {
    Q_OBJECT
public:
    using Handler = std::function<HttpResponse(const HttpRequest &)>;

    explicit MiniHttpServer(Handler handler, QObject *parent = nullptr)
        : QObject(parent)
        , server_(new QTcpServer(this))
        , handler_(std::move(handler)) {
        connect(server_, &QTcpServer::newConnection, this, &MiniHttpServer::onNewConnection);
        server_->listen(QHostAddress::LocalHost, 0);
    }

    QUrl baseUrl() const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1").arg(server_->serverPort()));
    }

    QVector<HttpRequest> requests;

private slots:
    void onNewConnection() {
        while (QTcpSocket *socket = server_->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                QByteArray &buffer = buffers_[socket];
                buffer.append(socket->readAll());

                const int headerEnd = buffer.indexOf("\r\n\r\n");
                if (headerEnd < 0) {
                    return;
                }

                const QByteArray headerBlock = buffer.left(headerEnd);
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

                const int totalExpected = headerEnd + 4 + contentLength;
                if (buffer.size() < totalExpected) {
                    return;
                }

                HttpRequest req;
                req.method = requestLine[0];
                req.target = requestLine[1];
                req.body = buffer.mid(headerEnd + 4, contentLength);
                requests.append(req);
                buffers_.remove(socket);

                const HttpResponse resp = handler_(req);
                QByteArray raw;
                raw += "HTTP/1.1 " + QByteArray::number(resp.statusCode) + " " + resp.reason + "\r\n";
                raw += "Content-Type: application/json\r\n";
                raw += "Content-Length: " + QByteArray::number(resp.body.size()) + "\r\n";
                raw += "Connection: close\r\n\r\n";
                raw += resp.body;

                socket->write(raw);
                socket->flush();
                socket->disconnectFromHost();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            });
        }
    }

private:
    QTcpServer *server_;
    Handler handler_;
    QHash<QTcpSocket *, QByteArray> buffers_;
};

} // namespace

class NetworkLayerTest : public QObject {
    Q_OBJECT

private slots:
    void testDtoTelemetryParsingAndNullHandling() {
        const QByteArray raw = R"({
            "deviceName": "Therm1",
            "deviceType": "Thermostat",
            "serialNumber": null,
            "timestamp": 1762605000123,
            "timestampIso": "2025-11-08T12:30:00.123000+00:00",
            "isOn": true,
            "currentTemperature": 21.5,
            "targetTemperature": 24.0,
            "currentHumidity": 0.0,
            "currentPower": 0.0,
            "brightness": 0,
            "valveLevel": 0,
            "targetValveLevel": 0,
            "currentValveLevel": 0,
            "mode": "HEAT"
        })";

        QString err;
        const auto parsed = DeviceTelemetry::fromJsonBytes(raw, &err);
        QVERIFY2(parsed.has_value(), qPrintable(err));
        QCOMPARE(parsed->deviceName, QStringLiteral("Therm1"));
        QCOMPARE(parsed->deviceType, QStringLiteral("Thermostat"));
        QCOMPARE(parsed->serialNumber, QString());
        QCOMPARE(parsed->timestamp, qint64(1762605000123));
        QCOMPARE(parsed->isOn, true);
        QCOMPARE(parsed->currentTemperature, 21.5);
        QCOMPARE(parsed->targetTemperature, 24.0);
        QCOMPARE(parsed->mode, QStringLiteral("HEAT"));
    }

    void testDtoDeviceStatusWithNullTelemetry() {
        const QByteArray raw = R"({
            "deviceName": "Plug1",
            "deviceType": "Plug",
            "topic": "home/plug1",
            "isOnline": false,
            "lastSeenMs": null,
            "telemetry": null
        })";
        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        const DeviceStatus status = DeviceStatus::fromJson(obj);
        QCOMPARE(status.deviceName, QStringLiteral("Plug1"));
        QCOMPARE(status.topic, QStringLiteral("home/plug1"));
        QCOMPARE(status.isOnline, false);
        QVERIFY(!status.lastSeenMs.has_value());
        QVERIFY(!status.telemetry.has_value());
    }

    void testDtoRuleItemRoundTrip() {
        RuleItem rule;
        rule.id = QStringLiteral("morning_warmup");
        rule.type = QStringLiteral("time_window");
        rule.enabled = true;
        rule.startTime = QStringLiteral("07:00:00");
        rule.endTime = QStringLiteral("09:00:00");
        rule.repeatDaily = true;
        rule.action = RuleAction{
            QStringLiteral("Therm1"),
            std::nullopt,
            QJsonObject{{QStringLiteral("command"), QStringLiteral("SET_TEMPERATURE")},
                        {QStringLiteral("value"), 23.5}}
        };

        const QJsonObject json = rule.toJson();
        QVERIFY(!json.contains(QStringLiteral("intervalMs")));
        QVERIFY(!json.contains(QStringLiteral("timestamp")));

        const RuleItem decoded = RuleItem::fromJson(json);
        QCOMPARE(decoded.id, QStringLiteral("morning_warmup"));
        QCOMPARE(decoded.type, QStringLiteral("time_window"));
        QCOMPARE(decoded.enabled, true);
        QCOMPARE(*decoded.startTime, QStringLiteral("07:00:00"));
        QCOMPARE(*decoded.endTime, QStringLiteral("09:00:00"));
        QCOMPARE(*decoded.repeatDaily, true);
        QVERIFY(decoded.action.has_value());
        QCOMPARE(*decoded.action->device, QStringLiteral("Therm1"));
    }

    void testRestClientAllEndpoints() {
        MiniHttpServer server([](const HttpRequest &req) -> HttpResponse {
            if (req.method == "GET" && req.target == "/api/v1/health") {
                return {200, "OK", R"({"status":"ok","service":"Smart Home API Gateway","version":"1.0.0","mqtt_broker":"localhost:1883","mqtt_connected":true,"sqlite_db_exists":true,"telemetry_records_count":42,"registered_devices_count":5,"online_devices":["Lamp1"],"active_ws_clients":1})"};
            }
            if (req.method == "GET" && req.target == "/api/v1/devices") {
                return {200, "OK", R"([{"deviceName":"Lamp1","deviceType":"Lamp","topic":"home/lamp1","isOnline":true,"lastSeenMs":1762605010500,"telemetry":{"deviceName":"Lamp1","deviceType":"Lamp","timestamp":1762605010500,"isOn":true,"brightness":85,"mode":"OFF"}}])"};
            }
            if (req.method == "POST" && req.target == "/api/v1/devices/Lamp1/command") {
                return {200, "OK", R"({"status":"sent","deviceName":"Lamp1","topic":"home/lamp1/set","payload":{"command":"SET_BRIGHTNESS","value":80}})"};
            }
            if (req.method == "GET" && req.target == "/api/v1/devices/Therm1/history?limit=50&start_ts=1000&end_ts=2000") {
                return {200, "OK", R"([{"deviceName":"Therm1","deviceType":"Thermostat","timestamp":1500,"isOn":true,"currentTemperature":22.0,"targetTemperature":23.0,"mode":"HEAT"}])"};
            }
            if (req.method == "GET" && req.target == "/api/v1/rules") {
                return {200, "OK", R"({"rules":[{"id":"r1","type":"point","enabled":true,"time":"23:30:00"}]})"};
            }
            if (req.method == "POST" && req.target == "/api/v1/rules/items") {
                return {201, "Created", req.body};
            }
            if (req.method == "PATCH" && req.target == "/api/v1/rules/items/r1") {
                return {200, "OK", R"({"id":"r1","type":"point","enabled":false,"time":"23:30:00"})"};
            }
            if (req.method == "DELETE" && req.target == "/api/v1/rules/items/r1") {
                return {200, "OK", R"({"rules":[]})"};
            }
            return {404, "Not Found", R"({"detail":"Unhandled mock route"})"};
        });

        ApiClient client(server.baseUrl());

        QSignalSpy healthSpy(&client, &ApiClient::healthReceived);
        client.getHealth();
        QVERIFY(healthSpy.wait(2000));
        QCOMPARE(healthSpy.first().first().value<HealthStatus>().mqttConnected, true);

        QSignalSpy devicesSpy(&client, &ApiClient::devicesReceived);
        client.getDevices();
        QVERIFY(devicesSpy.wait(2000));
        const auto devices = devicesSpy.first().first().value<QVector<DeviceStatus>>();
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.first().telemetry->brightness, 85);

        QSignalSpy cmdSpy(&client, &ApiClient::commandSent);
        client.sendCommand(QStringLiteral("Lamp1"), CommandRequest{QStringLiteral("SET_BRIGHTNESS"), 80.0, std::nullopt});
        QVERIFY(cmdSpy.wait(2000));
        QCOMPARE(cmdSpy.first().first().value<CommandResult>().status, QStringLiteral("sent"));

        QSignalSpy histSpy(&client, &ApiClient::deviceHistoryReceived);
        client.getDeviceHistory(QStringLiteral("Therm1"), 50, 1000, 2000);
        QVERIFY(histSpy.wait(2000));
        QCOMPARE(histSpy.first().at(1).value<QVector<DeviceTelemetry>>().size(), 1);

        QSignalSpy rulesSpy(&client, &ApiClient::rulesReceived);
        client.getRules();
        QVERIFY(rulesSpy.wait(2000));
        QCOMPARE(rulesSpy.first().first().value<QVector<RuleItem>>().size(), 1);

        QSignalSpy addSpy(&client, &ApiClient::ruleAdded);
        RuleItem added;
        added.id = QStringLiteral("r2");
        added.type = QStringLiteral("point");
        added.time = QStringLiteral("08:00:00");
        client.addRule(added);
        QVERIFY(addSpy.wait(2000));
        QCOMPARE(addSpy.first().first().value<RuleItem>().id, QStringLiteral("r2"));

        QSignalSpy toggleSpy(&client, &ApiClient::ruleToggled);
        client.toggleRule(QStringLiteral("r1"), false);
        QVERIFY(toggleSpy.wait(2000));
        QCOMPARE(toggleSpy.first().first().value<RuleItem>().enabled, false);

        QSignalSpy delSpy(&client, &ApiClient::ruleDeleted);
        client.deleteRule(QStringLiteral("r1"));
        QVERIFY(delSpy.wait(2000));
        QCOMPARE(delSpy.first().first().toString(), QStringLiteral("r1"));
        QCOMPARE(delSpy.first().at(1).value<QVector<RuleItem>>().size(), 0);
    }

    void testRestClientHttpErrorParsing() {
        MiniHttpServer server([](const HttpRequest &) -> HttpResponse {
            return {503, "Service Unavailable", R"({"detail":"MQTT broker is currently unavailable. Command cannot be delivered."})"};
        });

        ApiClient client(server.baseUrl());
        QSignalSpy errorSpy(&client, &ApiClient::requestFailed);
        client.sendCommand(QStringLiteral("Lamp1"), CommandRequest{QStringLiteral("ON"), std::nullopt, std::nullopt});
        QVERIFY(errorSpy.wait(2000));
        QCOMPARE(errorSpy.first().at(1).toInt(), 503);
        QVERIFY(errorSpy.first().at(2).toString().contains(QStringLiteral("MQTT broker is currently unavailable")));
    }

    void testWsClientSnapshotPingPongAndReconnect() {
        QWebSocketServer wsServer(QStringLiteral("TestGateway"), QWebSocketServer::NonSecureMode);
        QVERIFY(wsServer.listen(QHostAddress::LocalHost, 0));

        QWebSocket *serverSocket = nullptr;
        int connectionsCount = 0;
        bool respondToPing = true;

        connect(&wsServer, &QWebSocketServer::newConnection, this, [&]() {
            serverSocket = wsServer.nextPendingConnection();
            ++connectionsCount;
            // Send initial snapshot immediately upon connect
            serverSocket->sendTextMessage(QStringLiteral(
                R"({"deviceName":"Lamp1","deviceType":"Lamp","timestamp":1762605099000,"isOn":true,"brightness":95,"mode":"OFF"})"));

            connect(serverSocket, &QWebSocket::textMessageReceived, this, [&, sock = serverSocket](const QString &msg) {
                if (msg == QStringLiteral("ping") && respondToPing) {
                    sock->sendTextMessage(QStringLiteral("pong"));
                }
            });
        });

        const QUrl wsUrl(QStringLiteral("ws://127.0.0.1:%1/ws/telemetry").arg(wsServer.serverPort()));
        WsClient client(wsUrl);
        client.setReconnectIntervals(80, 300);
        client.setPingIntervals(0, 150);

        QSignalSpy connectedSpy(&client, &WsClient::connected);
        QSignalSpy telemetrySpy(&client, &WsClient::telemetryReceived);
        QSignalSpy pongSpy(&client, &WsClient::pongReceived);
        QSignalSpy reconnectingSpy(&client, &WsClient::reconnecting);

        client.connectToGateway();
        QVERIFY(connectedSpy.wait(2000));
        QTRY_COMPARE_WITH_TIMEOUT(telemetrySpy.count(), 1, 2000);
        QCOMPARE(telemetrySpy.first().first().value<DeviceTelemetry>().deviceName, QStringLiteral("Lamp1"));
        QCOMPARE(telemetrySpy.first().first().value<DeviceTelemetry>().brightness, 95);

        // Verify ping -> pong
        client.sendPing();
        QVERIFY(pongSpy.wait(2000));

        // Simulate missing pong -> timeout -> disconnect -> auto-reconnect
        respondToPing = false;
        client.sendPing();
        QVERIFY(reconnectingSpy.wait(2000));
        QVERIFY(connectedSpy.wait(2000));
        QVERIFY(connectionsCount >= 2);

        client.disconnectFromGateway();
    }
};

QTEST_MAIN(NetworkLayerTest)
#include "test_NetworkLayer.moc"
