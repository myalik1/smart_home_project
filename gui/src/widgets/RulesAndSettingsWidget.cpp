#include "widgets/RulesAndSettingsWidget.h"
#include "widgets/AddRuleDialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace smarthome::ui {

RulesAndSettingsWidget::RulesAndSettingsWidget(const QUrl &initialGatewayUrl, QWidget *parent)
    : QWidget(parent)
    , rulesSummaryLabel_(new QLabel(tr("Всего правил: 0 | Активно: 0"), this))
    , addRuleButton_(new QPushButton(tr("Добавить правило"), this))
    , refreshRulesButton_(new QPushButton(tr("Обновить"), this))
    , rulesTable_(new QTableWidget(this))
    , gatewayUrlEdit_(new QLineEdit(this))
    , applyUrlButton_(new QPushButton(tr("Применить"), this))
    , diagStatusLabel_(new QLabel(tr("Проверка..."), this))
    , diagMqttLabel_(new QLabel(tr("—"), this))
    , diagDbPathLabel_(new QLabel(tr("—"), this))
    , diagRecordsLabel_(new QLabel(tr("0"), this))
    , diagRulesPathLabel_(new QLabel(tr("—"), this))
    , diagClientsLabel_(new QLabel(tr("—"), this)) {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    setupRulesSection(rootLayout);
    setupSettingsSection(rootLayout, initialGatewayUrl);
}

void RulesAndSettingsWidget::setupRulesSection(QVBoxLayout *rootLayout) {
    auto *rulesGroup = new QGroupBox(tr("Правила автоматизации (data/rules.json)"), this);
    rulesGroup->setObjectName(QStringLiteral("rulesSectionGroup"));

    rulesSummaryLabel_->setObjectName(QStringLiteral("rulesSummaryLabel"));
    addRuleButton_->setObjectName(QStringLiteral("addRuleButton"));
    refreshRulesButton_->setObjectName(QStringLiteral("refreshRulesButton"));

    auto *toolbar = new QHBoxLayout();
    toolbar->addWidget(rulesSummaryLabel_);
    toolbar->addStretch(1);
    toolbar->addWidget(addRuleButton_);
    toolbar->addWidget(refreshRulesButton_);

    rulesTable_->setObjectName(QStringLiteral("rulesTable"));
    rulesTable_->setColumnCount(7);
    rulesTable_->setHorizontalHeaderLabels({
        tr("ID"),
        tr("Тип"),
        tr("Расписание"),
        tr("Устройство"),
        tr("Команда"),
        tr("Активно"),
        tr("Удаление"),
    });
    rulesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rulesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rulesTable_->setAlternatingRowColors(true);
    rulesTable_->verticalHeader()->setVisible(false);
    rulesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    rulesTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    auto *groupLayout = new QVBoxLayout(rulesGroup);
    groupLayout->setSpacing(8);
    groupLayout->addLayout(toolbar);
    groupLayout->addWidget(rulesTable_, 1);

    rootLayout->addWidget(rulesGroup, 3);

    connect(addRuleButton_, &QPushButton::clicked, this, &RulesAndSettingsWidget::onAddRuleClicked);
    connect(refreshRulesButton_, &QPushButton::clicked, this, &RulesAndSettingsWidget::refreshRulesRequested);
}

void RulesAndSettingsWidget::setupSettingsSection(QVBoxLayout *rootLayout, const QUrl &initialGatewayUrl) {
    auto *settingsGroup = new QGroupBox(tr("Настройки подключения и информация"), this);
    settingsGroup->setObjectName(QStringLiteral("settingsSectionGroup"));

    gatewayUrlEdit_->setObjectName(QStringLiteral("gatewayUrlEdit"));
    gatewayUrlEdit_->setPlaceholderText(QStringLiteral("http://127.0.0.1:8001"));
    setGatewayUrl(initialGatewayUrl);

    applyUrlButton_->setObjectName(QStringLiteral("applyGatewayUrlButton"));

    auto *urlRow = new QHBoxLayout();
    urlRow->addWidget(gatewayUrlEdit_, 1);
    urlRow->addWidget(applyUrlButton_);

    diagStatusLabel_->setObjectName(QStringLiteral("diagStatusValue"));
    diagMqttLabel_->setObjectName(QStringLiteral("diagMqttValue"));
    diagDbPathLabel_->setObjectName(QStringLiteral("diagDbPathValue"));
    diagDbPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    diagRecordsLabel_->setObjectName(QStringLiteral("diagRecordsValue"));
    diagRulesPathLabel_->setObjectName(QStringLiteral("diagRulesPathValue"));
    diagRulesPathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    diagClientsLabel_->setObjectName(QStringLiteral("diagClientsValue"));

    auto *form = new QFormLayout(settingsGroup);
    form->setSpacing(6);
    form->addRow(tr("URL шлюза:"), urlRow);
    form->addRow(tr("Состояние шлюза:"), diagStatusLabel_);
    form->addRow(tr("Статус MQTT:"), diagMqttLabel_);
    form->addRow(tr("База SQLite:"), diagDbPathLabel_);
    form->addRow(tr("Записей телеметрии:"), diagRecordsLabel_);
    form->addRow(tr("Конфигурация правил:"), diagRulesPathLabel_);
    form->addRow(tr("Подключения:"), diagClientsLabel_);

    rootLayout->addWidget(settingsGroup, 1);

    connect(applyUrlButton_, &QPushButton::clicked, this, &RulesAndSettingsWidget::onApplyGatewayUrlClicked);
    connect(gatewayUrlEdit_, &QLineEdit::returnPressed, this, &RulesAndSettingsWidget::onApplyGatewayUrlClicked);
}

QString RulesAndSettingsWidget::gatewayUrlText() const {
    return gatewayUrlEdit_->text().trimmed();
}

AddRuleDialog *RulesAndSettingsWidget::activeAddRuleDialog() const {
    return activeDialog_;
}

QCheckBox *RulesAndSettingsWidget::rowEnabledCheckBox(int row) const {
    QWidget *container = rulesTable_->cellWidget(row, 5);
    return container ? container->findChild<QCheckBox *>() : nullptr;
}

QPushButton *RulesAndSettingsWidget::rowDeleteButton(int row) const {
    QWidget *container = rulesTable_->cellWidget(row, 6);
    return container ? container->findChild<QPushButton *>() : nullptr;
}

QString RulesAndSettingsWidget::diagStatusText() const {
    return diagStatusLabel_->text();
}

QString RulesAndSettingsWidget::diagMqttText() const {
    return diagMqttLabel_->text();
}

QString RulesAndSettingsWidget::diagDbPathText() const {
    return diagDbPathLabel_->text();
}

QString RulesAndSettingsWidget::diagRecordsCountText() const {
    return diagRecordsLabel_->text();
}

void RulesAndSettingsWidget::setAvailableDevices(const QVector<smarthome::net::DeviceStatus> &devices) {
    QStringList names;
    names.reserve(devices.size());
    for (const auto &dev : devices) {
        if (!dev.deviceName.isEmpty() && !names.contains(dev.deviceName)) {
            names.append(dev.deviceName);
        }
    }
    if (!names.isEmpty()) {
        deviceNames_ = names;
    }
}

void RulesAndSettingsWidget::setRules(const QVector<smarthome::net::RuleItem> &rules) {
    rules_ = rules;
    renderRulesTable();
}

void RulesAndSettingsWidget::onRuleAdded(const smarthome::net::RuleItem &rule) {
    if (activeDialog_) {
        activeDialog_->handleSubmitSuccess();
    }

    bool replaced = false;
    for (auto &existing : rules_) {
        if (existing.id == rule.id) {
            existing = rule;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        rules_.append(rule);
    }
    renderRulesTable();
}

void RulesAndSettingsWidget::onRuleToggled(const smarthome::net::RuleItem &rule) {
    for (auto &existing : rules_) {
        if (existing.id == rule.id) {
            existing = rule;
            break;
        }
    }
    renderRulesTable();
}

void RulesAndSettingsWidget::onRuleDeleted(const QString & /*ruleId*/,
                                           const QVector<smarthome::net::RuleItem> &remainingRules) {
    rules_ = remainingRules;
    renderRulesTable();
}

void RulesAndSettingsWidget::onApiError(const QString &endpoint, int httpStatus, const QString &errorMessage) {
    if (activeDialog_ && endpoint == QStringLiteral("/rules/items")) {
        activeDialog_->handleSubmitError(httpStatus, errorMessage);
    } else if (endpoint.startsWith(QStringLiteral("/rules/items/"))) {
        renderRulesTable();
    }
}

void RulesAndSettingsWidget::setHealthStatus(const smarthome::net::HealthStatus &health) {
    const QString statusText = health.version.isEmpty()
        ? health.status
        : QStringLiteral("%1 (v%2)").arg(health.status, health.version);
    diagStatusLabel_->setText(statusText);

    const QString mqttState = health.mqttConnected ? tr("Подключён") : tr("Отключён");
    diagMqttLabel_->setText(health.mqttBroker.isEmpty()
                                ? mqttState
                                : QStringLiteral("%1 (%2)").arg(mqttState, health.mqttBroker));

    const QString dbExistsText = health.sqliteDbExists ? tr("найдена") : tr("не создана");
    diagDbPathLabel_->setText(health.sqliteDbPath.isEmpty()
                                  ? QStringLiteral("—")
                                  : QStringLiteral("%1 (%2)").arg(health.sqliteDbPath, dbExistsText));

    diagRecordsLabel_->setText(QString::number(health.telemetryRecordsCount));
    diagRulesPathLabel_->setText(health.rulesConfigPath.isEmpty() ? QStringLiteral("—") : health.rulesConfigPath);
    diagClientsLabel_->setText(
        tr("Устройств: %1 (онлайн: %2) | Активных WS-клиентов: %3")
            .arg(health.registeredDevicesCount)
            .arg(health.onlineDevices.size())
            .arg(health.activeWsClients));
}

void RulesAndSettingsWidget::setGatewayOffline() {
    diagStatusLabel_->setText(tr("Недоступен (Offline)"));
    diagMqttLabel_->setText(tr("Неизвестно"));
}

void RulesAndSettingsWidget::setGatewayUrl(const QUrl &url) {
    QUrl origin = url;
    QString path = origin.path();
    if (path.endsWith(QStringLiteral("/api/v1"))) {
        path.chop(7);
        origin.setPath(path);
    }
    if (origin.path() == QStringLiteral("/")) {
        origin.setPath(QString());
    }
    gatewayUrlEdit_->setText(origin.toString());
}

void RulesAndSettingsWidget::onAddRuleClicked() {
    auto *dialog = new AddRuleDialog(deviceNames_, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    activeDialog_ = dialog;

    connect(dialog, &AddRuleDialog::ruleSubmitted,
            this, &RulesAndSettingsWidget::addRuleRequested);

    dialog->exec();
}

void RulesAndSettingsWidget::onApplyGatewayUrlClicked() {
    const QString raw = gatewayUrlEdit_->text().trimmed();
    const QUrl parsed = QUrl::fromUserInput(raw);
    const QString scheme = parsed.scheme().toLower();
    if (!parsed.isValid() || parsed.host().isEmpty()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        emit invalidGatewayUrlEntered(tr("Укажите корректный HTTP/HTTPS URL шлюза, например http://127.0.0.1:8001"));
        return;
    }

    QUrl origin;
    origin.setScheme(scheme);
    origin.setHost(parsed.host());
    if (parsed.port() != -1) {
        origin.setPort(parsed.port());
    }
    gatewayUrlEdit_->setText(origin.toString());
    emit gatewayUrlApplyRequested(origin);
}

void RulesAndSettingsWidget::confirmAndDeleteRule(const QString &ruleId) {
    const auto reply = QMessageBox::question(
        this,
        tr("Удаление правила"),
        tr("Вы действительно хотите удалить правило '%1'?").arg(ruleId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        emit deleteRuleRequested(ruleId);
    }
}

void RulesAndSettingsWidget::renderRulesTable() {
    updatingTable_ = true;
    rulesTable_->setRowCount(0);
    rulesTable_->setRowCount(rules_.size());

    int activeCount = 0;
    for (int row = 0; row < rules_.size(); ++row) {
        const auto &rule = rules_.at(row);
        if (rule.enabled) {
            ++activeCount;
        }

        rulesTable_->setItem(row, 0, new QTableWidgetItem(rule.id));
        rulesTable_->setItem(row, 1, new QTableWidgetItem(rule.type));
        rulesTable_->setItem(row, 2, new QTableWidgetItem(formatSchedule(rule)));
        rulesTable_->setItem(row, 3, new QTableWidgetItem(formatTargetDevice(rule)));
        rulesTable_->setItem(row, 4, new QTableWidgetItem(formatCommandSummary(rule)));

        auto *checkHost = new QWidget(rulesTable_);
        auto *checkLayout = new QHBoxLayout(checkHost);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        auto *enabledCheck = new QCheckBox(checkHost);
        enabledCheck->setObjectName(QStringLiteral("ruleEnabledCheckBox"));
        enabledCheck->setChecked(rule.enabled);
        checkLayout->addWidget(enabledCheck);
        rulesTable_->setCellWidget(row, 5, checkHost);

        connect(enabledCheck, &QCheckBox::toggled, this, [this, ruleId = rule.id](bool checked) {
            if (!updatingTable_) {
                emit toggleRuleRequested(ruleId, checked);
            }
        });

        auto *delHost = new QWidget(rulesTable_);
        auto *delLayout = new QHBoxLayout(delHost);
        delLayout->setContentsMargins(4, 2, 4, 2);
        delLayout->setAlignment(Qt::AlignCenter);
        auto *deleteBtn = new QPushButton(tr("Удалить"), delHost);
        deleteBtn->setObjectName(QStringLiteral("deleteRuleButton"));
        delLayout->addWidget(deleteBtn);
        rulesTable_->setCellWidget(row, 6, delHost);

        connect(deleteBtn, &QPushButton::clicked, this, [this, ruleId = rule.id]() {
            confirmAndDeleteRule(ruleId);
        });
    }

    rulesSummaryLabel_->setText(
        tr("Всего правил: %1 | Активно: %2").arg(rules_.size()).arg(activeCount));
    updatingTable_ = false;
}

QString RulesAndSettingsWidget::formatSchedule(const smarthome::net::RuleItem &rule) {
    if (rule.type == QStringLiteral("point") || rule.type == QStringLiteral("time_of_day")) {
        const QString base = rule.time.value_or(QStringLiteral("-"));
        return rule.repeatDaily.value_or(false) ? tr("%1 (ежедн.)").arg(base) : base;
    }
    if (rule.type == QStringLiteral("time_window") || rule.type == QStringLiteral("interval_window")) {
        return QStringLiteral("%1 - %2")
            .arg(rule.startTime.value_or(QStringLiteral("?")),
                 rule.endTime.value_or(QStringLiteral("?")));
    }
    if (rule.type == QStringLiteral("periodic")) {
        if (rule.intervalSeconds.has_value()) {
            return tr("каждые %1 сек").arg(*rule.intervalSeconds);
        }
        if (rule.intervalMs.has_value()) {
            return tr("каждые %1 мс").arg(*rule.intervalMs);
        }
        return tr("периодический");
    }
    if (rule.type == QStringLiteral("timestamp") || rule.type == QStringLiteral("point_absolute")) {
        if (rule.timestamp.has_value()) {
            const qint64 ms = (*rule.timestamp < 10000000000LL) ? (*rule.timestamp * 1000) : *rule.timestamp;
            return QDateTime::fromMSecsSinceEpoch(ms).toLocalTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
        }
        return QStringLiteral("-");
    }
    return QStringLiteral("-");
}

QString RulesAndSettingsWidget::formatTargetDevice(const smarthome::net::RuleItem &rule) {
    const auto &act = rule.startAction.has_value() ? rule.startAction : rule.action;
    if (!act.has_value()) {
        return QStringLiteral("-");
    }
    if (act->device.has_value() && !act->device->isEmpty()) {
        return *act->device;
    }
    if (act->topic.has_value() && !act->topic->isEmpty()) {
        return *act->topic;
    }
    return QStringLiteral("-");
}

QString RulesAndSettingsWidget::formatCommandSummary(const smarthome::net::RuleItem &rule) {
    const auto &act = rule.startAction.has_value() ? rule.startAction : rule.action;
    if (!act.has_value()) {
        return QStringLiteral("-");
    }
    if (act->command.isString()) {
        return act->command.toString();
    }
    if (act->command.isObject()) {
        const QJsonObject obj = act->command.toObject();
        const QString cmd = obj.value(QStringLiteral("command")).toString();
        if (obj.contains(QStringLiteral("value"))) {
            return QStringLiteral("%1 (%2)").arg(cmd, obj.value(QStringLiteral("value")).toVariant().toString());
        }
        if (obj.contains(QStringLiteral("mode"))) {
            return QStringLiteral("%1 (%2)").arg(cmd, obj.value(QStringLiteral("mode")).toString());
        }
        if (!cmd.isEmpty()) {
            return cmd;
        }
        return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
    return QStringLiteral("-");
}

} // namespace smarthome::ui
