#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "net/ApiClient.h"
#include "net/WsClient.h"
#include "widgets/DashboardWidget.h"

#include <QChart>
#include <QChartView>
#include <QFile>
#include <QLabel>
#include <QLineSeries>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(QUrl(QStringLiteral("http://127.0.0.1:8001/api/v1")),
                 QUrl(QStringLiteral("ws://127.0.0.1:8001/ws/telemetry")),
                 5000,
                 parent) {}

MainWindow::MainWindow(const QUrl &restBaseUrl,
                       const QUrl &wsUrl,
                       int healthPollIntervalMs,
                       QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , apiClient_(new smarthome::net::ApiClient(restBaseUrl, this))
    , wsClient_(new smarthome::net::WsClient(wsUrl, this))
    , dashboardWidget_(new smarthome::ui::DashboardWidget(this))
    , chartView_(nullptr)
    , healthPollTimer_(new QTimer(this))
    , gatewayStatusLabel_(new QLabel(this))
    , mqttStatusLabel_(new QLabel(this))
    , wsStatusLabel_(new QLabel(this))
    , healthPollIntervalMs_(qMax(500, healthPollIntervalMs)) {
    ui->setupUi(this);
    loadStylesheet();
    setupDashboardTab();
    setupStatusBarIndicators();
    setupTelemetryPreviewChart();
    setupConnections();
    startGatewaySync();
}

MainWindow::~MainWindow() {
    delete ui;
}

QString MainWindow::gatewayStatusText() const {
    return gatewayStatusLabel_->text();
}

QString MainWindow::mqttStatusText() const {
    return mqttStatusLabel_->text();
}

QString MainWindow::wsStatusText() const {
    return wsStatusLabel_->text();
}

void MainWindow::setupDashboardTab() {
    ui->dashboardTabLayout->addWidget(dashboardWidget_);
}

void MainWindow::setupStatusBarIndicators() {
    gatewayStatusLabel_->setObjectName(QStringLiteral("statusBadge"));
    mqttStatusLabel_->setObjectName(QStringLiteral("statusBadge"));
    wsStatusLabel_->setObjectName(QStringLiteral("statusBadge"));

    updateBadgeState(gatewayStatusLabel_, tr("Шлюз: проверка..."), QStringLiteral("warn"));
    updateBadgeState(mqttStatusLabel_, tr("MQTT: —"), QStringLiteral("warn"));
    updateBadgeState(wsStatusLabel_, tr("WS: подключение..."), QStringLiteral("warn"));

    statusBar()->addPermanentWidget(gatewayStatusLabel_);
    statusBar()->addPermanentWidget(mqttStatusLabel_);
    statusBar()->addPermanentWidget(wsStatusLabel_);
}

void MainWindow::setupTelemetryPreviewChart() {
    auto *series = new QLineSeries(this);
    series->setName(tr("Telemetry Preview"));
    series->append(0, 21.5);
    series->append(1, 22.0);
    series->append(2, 22.4);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->setTitle(tr("Smart Home Telemetry"));

    chartView_ = new QChartView(chart, ui->chartContainer);
    chartView_->setRenderHint(QPainter::Antialiasing);

    auto *layout = new QVBoxLayout(ui->chartContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(chartView_);
}

void MainWindow::setupConnections() {
    connect(apiClient_, &smarthome::net::ApiClient::devicesReceived,
            dashboardWidget_, &smarthome::ui::DashboardWidget::setDevices);
    connect(apiClient_, &smarthome::net::ApiClient::healthReceived,
            this, &MainWindow::onHealthReceived);
    connect(apiClient_, &smarthome::net::ApiClient::requestFailed,
            this, &MainWindow::onApiRequestFailed);

    connect(dashboardWidget_, &smarthome::ui::DashboardWidget::refreshRequested,
            apiClient_, &smarthome::net::ApiClient::getDevices);

    connect(wsClient_, &smarthome::net::WsClient::telemetryReceived,
            dashboardWidget_, &smarthome::ui::DashboardWidget::updateTelemetry);
    connect(wsClient_, &smarthome::net::WsClient::connected,
            this, &MainWindow::onWsConnected);
    connect(wsClient_, &smarthome::net::WsClient::disconnected,
            this, &MainWindow::onWsDisconnected);
    connect(wsClient_, &smarthome::net::WsClient::reconnecting,
            this, &MainWindow::onWsReconnecting);

    connect(healthPollTimer_, &QTimer::timeout,
            apiClient_, &smarthome::net::ApiClient::getHealth);
}

void MainWindow::startGatewaySync() {
    apiClient_->getDevices();
    apiClient_->getHealth();
    healthPollTimer_->start(healthPollIntervalMs_);
    wsClient_->connectToGateway();
}

void MainWindow::onHealthReceived(const smarthome::net::HealthStatus &health) {
    const QString gwText = health.version.isEmpty()
        ? tr("Шлюз: Online")
        : tr("Шлюз: Online (v%1)").arg(health.version);
    updateBadgeState(gatewayStatusLabel_, gwText, QStringLiteral("ok"));

    if (health.mqttConnected) {
        updateBadgeState(mqttStatusLabel_, tr("MQTT: Подключён"), QStringLiteral("ok"));
    } else {
        updateBadgeState(mqttStatusLabel_, tr("MQTT: Отключён"), QStringLiteral("error"));
    }
}

void MainWindow::onApiRequestFailed(const QString &endpoint, int httpStatus, const QString &errorMessage) {
    if (endpoint == QStringLiteral("/health") || httpStatus == 0) {
        updateBadgeState(gatewayStatusLabel_, tr("Шлюз: Offline"), QStringLiteral("error"));
        updateBadgeState(mqttStatusLabel_, tr("MQTT: Неизвестно"), QStringLiteral("warn"));
    }
    statusBar()->showMessage(tr("Ошибка %1: %2").arg(endpoint, errorMessage), 5000);
}

void MainWindow::onWsConnected() {
    updateBadgeState(wsStatusLabel_, tr("WS: Подключён"), QStringLiteral("ok"));
}

void MainWindow::onWsDisconnected() {
    updateBadgeState(wsStatusLabel_, tr("WS: Отключён"), QStringLiteral("error"));
}

void MainWindow::onWsReconnecting(int attempt, int /*delayMs*/) {
    updateBadgeState(wsStatusLabel_, tr("WS: Повтор #%1").arg(attempt), QStringLiteral("warn"));
}

void MainWindow::updateBadgeState(QLabel *label, const QString &text, const QString &state) {
    label->setText(text);
    label->setProperty("statusState", state);
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

void MainWindow::loadStylesheet() {
    QFile styleFile(QStringLiteral(":/styles/app.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
}
