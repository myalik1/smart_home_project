#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "net/ApiClient.h"
#include "net/WsClient.h"
#include "widgets/AnalyticsWidget.h"
#include "widgets/DashboardWidget.h"
#include "widgets/RulesAndSettingsWidget.h"

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>

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
    , analyticsWidget_(new smarthome::ui::AnalyticsWidget(this))
    , rulesSettingsWidget_(new smarthome::ui::RulesAndSettingsWidget(restBaseUrl, this))
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
    setupAnalyticsTab();
    setupRulesSettingsTab();
    setupStatusBarIndicators();
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

void MainWindow::setupAnalyticsTab() {
    ui->analyticsTabLayout->addWidget(analyticsWidget_);
}

void MainWindow::setupRulesSettingsTab() {
    ui->rulesSettingsTabLayout->addWidget(rulesSettingsWidget_);
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

void MainWindow::setupConnections() {
    connect(apiClient_, &smarthome::net::ApiClient::devicesReceived,
            dashboardWidget_, &smarthome::ui::DashboardWidget::setDevices);
    connect(apiClient_, &smarthome::net::ApiClient::devicesReceived,
            analyticsWidget_, &smarthome::ui::AnalyticsWidget::setAvailableDevices);
    connect(apiClient_, &smarthome::net::ApiClient::devicesReceived,
            rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::setAvailableDevices);
    connect(apiClient_, &smarthome::net::ApiClient::deviceHistoryReceived,
            analyticsWidget_, &smarthome::ui::AnalyticsWidget::setDeviceHistory);
    connect(apiClient_, &smarthome::net::ApiClient::allHistoryReceived,
            analyticsWidget_, &smarthome::ui::AnalyticsWidget::setAllHistory);

    connect(apiClient_, &smarthome::net::ApiClient::rulesReceived,
            rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::setRules);
    connect(apiClient_, &smarthome::net::ApiClient::ruleAdded,
            this, [this](const smarthome::net::RuleItem &rule) {
                rulesSettingsWidget_->onRuleAdded(rule);
                statusBar()->showMessage(tr("Правило '%1' добавлено").arg(rule.id), 3000);
            });
    connect(apiClient_, &smarthome::net::ApiClient::ruleToggled,
            this, [this](const smarthome::net::RuleItem &rule) {
                rulesSettingsWidget_->onRuleToggled(rule);
                statusBar()->showMessage(
                    tr("Правило '%1' %2").arg(rule.id, rule.enabled ? tr("включено") : tr("выключено")), 3000);
            });
    connect(apiClient_, &smarthome::net::ApiClient::ruleDeleted,
            this, [this](const QString &ruleId, const QVector<smarthome::net::RuleItem> &remaining) {
                rulesSettingsWidget_->onRuleDeleted(ruleId, remaining);
                statusBar()->showMessage(tr("Правило '%1' удалено").arg(ruleId), 3000);
            });

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

    connect(analyticsWidget_, &smarthome::ui::AnalyticsWidget::deviceHistoryRequested,
            this, [this](const QString &deviceName, qint64 startTsMs, qint64 endTsMs, int limit) {
                apiClient_->getDeviceHistory(deviceName, limit, startTsMs, endTsMs);
            });
    connect(analyticsWidget_, &smarthome::ui::AnalyticsWidget::allHistoryRequested,
            apiClient_, &smarthome::net::ApiClient::getAllHistory);

    connect(rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::refreshRulesRequested,
            apiClient_, &smarthome::net::ApiClient::getRules);
    connect(rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::addRuleRequested,
            apiClient_, &smarthome::net::ApiClient::addRule);
    connect(rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::toggleRuleRequested,
            apiClient_, &smarthome::net::ApiClient::toggleRule);
    connect(rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::deleteRuleRequested,
            apiClient_, &smarthome::net::ApiClient::deleteRule);
    connect(rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::gatewayUrlApplyRequested,
            this, &MainWindow::onGatewayUrlApplyRequested);
    connect(rulesSettingsWidget_, &smarthome::ui::RulesAndSettingsWidget::invalidGatewayUrlEntered,
            this, [this](const QString &msg) {
                showAlertBanner(msg);
                statusBar()->showMessage(msg, 5000);
            });

    connect(ui->mainTabWidget, &QTabWidget::currentChanged, this, [this](int idx) {
        QWidget *tab = ui->mainTabWidget->widget(idx);
        if (tab == ui->analyticsTab) {
            analyticsWidget_->requestRefresh();
        } else if (tab == ui->rulesSettingsTab) {
            apiClient_->getRules();
            apiClient_->getHealth();
        }
    });

    connect(wsClient_, &smarthome::net::WsClient::telemetryReceived,
            dashboardWidget_, &smarthome::ui::DashboardWidget::updateTelemetry);
    connect(wsClient_, &smarthome::net::WsClient::telemetryReceived,
            analyticsWidget_, &smarthome::ui::AnalyticsWidget::appendLiveTelemetry);
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
    apiClient_->getRules();
    analyticsWidget_->requestRefresh();
    healthPollTimer_->start(healthPollIntervalMs_);
    wsClient_->connectToGateway();
}

void MainWindow::onGatewayUrlApplyRequested(const QUrl &gatewayOrigin) {
    QUrl restUrl = gatewayOrigin;
    restUrl.setPath(QStringLiteral("/api/v1"));

    QUrl wsUrl = gatewayOrigin;
    wsUrl.setScheme(gatewayOrigin.scheme().toLower() == QStringLiteral("https")
                        ? QStringLiteral("wss")
                        : QStringLiteral("ws"));
    wsUrl.setPath(QStringLiteral("/ws/telemetry"));

    hideAlertBanner();
    apiClient_->setBaseUrl(restUrl);
    wsClient_->connectToGateway(wsUrl);

    apiClient_->getDevices();
    apiClient_->getHealth();
    apiClient_->getRules();
    analyticsWidget_->requestRefresh();

    statusBar()->showMessage(tr("URL шлюза обновлён: %1").arg(gatewayOrigin.toString()), 4000);
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

    rulesSettingsWidget_->setHealthStatus(health);
}

void MainWindow::onCommandSent(const smarthome::net::CommandResult &result) {
    hideAlertBanner();
    const QString cmd = result.payload.value(QStringLiteral("command")).toString();
    statusBar()->showMessage(
        tr("Команда %1 отправлена на %2").arg(cmd, result.deviceName), 3000);
}

void MainWindow::onApiRequestFailed(const QString &endpoint, int httpStatus, const QString &errorMessage) {
    rulesSettingsWidget_->onApiError(endpoint, httpStatus, errorMessage);

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
        rulesSettingsWidget_->setGatewayOffline();
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
