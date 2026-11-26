#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "net/ApiClient.h"
#include "net/WsClient.h"
#include "widgets/DashboardWidget.h"

#include <QChart>
#include <QChartView>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QPushButton>
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
    , alertBanner_(new QFrame(this))
    , alertLabel_(new QLabel(alertBanner_))
    , alertCloseButton_(new QPushButton(tr("Закрыть"), alertBanner_))
    , gatewayStatusLabel_(new QLabel(this))
    , mqttStatusLabel_(new QLabel(this))
    , wsStatusLabel_(new QLabel(this))
    , healthPollIntervalMs_(qMax(500, healthPollIntervalMs)) {
    ui->setupUi(this);
    loadStylesheet();
    setupAlertBanner();
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

bool MainWindow::isAlertBannerVisible() const {
    return !alertBanner_->isHidden();
}

QString MainWindow::alertBannerText() const {
    return alertLabel_->text();
}

void MainWindow::setupAlertBanner() {
    alertBanner_->setObjectName(QStringLiteral("alertBanner"));
    alertLabel_->setObjectName(QStringLiteral("alertBannerLabel"));
    alertLabel_->setWordWrap(true);
    alertCloseButton_->setObjectName(QStringLiteral("alertBannerClose"));

    auto *bannerLayout = new QHBoxLayout(alertBanner_);
    bannerLayout->setContentsMargins(12, 8, 12, 8);
    bannerLayout->addWidget(alertLabel_, 1);
    bannerLayout->addWidget(alertCloseButton_);

    connect(alertCloseButton_, &QPushButton::clicked, this, &MainWindow::hideAlertBanner);

    alertBanner_->hide();
    ui->verticalLayout->insertWidget(1, alertBanner_);
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
    connect(apiClient_, &smarthome::net::ApiClient::commandSent,
            this, &MainWindow::onCommandSent);
    connect(apiClient_, &smarthome::net::ApiClient::requestFailed,
            this, &MainWindow::onApiRequestFailed);

    connect(dashboardWidget_, &smarthome::ui::DashboardWidget::refreshRequested,
            apiClient_, &smarthome::net::ApiClient::getDevices);
    connect(dashboardWidget_, &smarthome::ui::DashboardWidget::commandRequested,
            apiClient_, &smarthome::net::ApiClient::sendCommand);

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
        hideAlertBanner();
    } else {
        updateBadgeState(mqttStatusLabel_, tr("MQTT: Отключён"), QStringLiteral("error"));
    }
}

void MainWindow::onCommandSent(const smarthome::net::CommandResult &result) {
    hideAlertBanner();
    const QString cmd = result.payload.value(QStringLiteral("command")).toString();
    statusBar()->showMessage(
        tr("Команда %1 отправлена на %2").arg(cmd, result.deviceName), 3000);
}

void MainWindow::onApiRequestFailed(const QString &endpoint, int httpStatus, const QString &errorMessage) {
    if (httpStatus == 503) {
        const QString msg = tr("Нет связи с MQTT-брокером: команда не доставлена");
        showAlertBanner(msg);
        statusBar()->showMessage(msg, 6000);
        if (endpoint != QStringLiteral("/health")) {
            updateBadgeState(mqttStatusLabel_, tr("MQTT: Отключён"), QStringLiteral("error"));
            return;
        }
    }

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

void MainWindow::showAlertBanner(const QString &message) {
    alertLabel_->setText(message);
    alertBanner_->show();
}

void MainWindow::hideAlertBanner() {
    if (!alertBanner_->isHidden()) {
        alertBanner_->hide();
    }
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
