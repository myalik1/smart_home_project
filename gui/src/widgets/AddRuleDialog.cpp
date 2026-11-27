#include "widgets/AddRuleDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace smarthome::ui {

AddRuleDialog::AddRuleDialog(const QStringList &deviceNames, QWidget *parent)
    : QDialog(parent)
    , idEdit_(new QLineEdit(this))
    , typeCombo_(new QComboBox(this))
    , enabledCheck_(new QCheckBox(tr("Правило активно сразу после создания"), this))
    , scheduleStack_(new QStackedWidget(this))
    , pointTimeEdit_(new QTimeEdit(QTime(19, 0, 0), this))
    , repeatDailyCheck_(new QCheckBox(tr("Повторять ежедневно"), this))
    , startTimeEdit_(new QTimeEdit(QTime(7, 0, 0), this))
    , endTimeEdit_(new QTimeEdit(QTime(9, 0, 0), this))
    , windowRepeatDailyCheck_(new QCheckBox(tr("Повторять ежедневно"), this))
    , intervalSecondsSpin_(new QSpinBox(this))
    , maxTicksSpin_(new QSpinBox(this))
    , timestampEdit_(new QDateTimeEdit(QDateTime::currentDateTime().addSecs(300), this))
    , deviceCombo_(new QComboBox(this))
    , commandCombo_(new QComboBox(this))
    , valueLabel_(new QLabel(tr("Значение:"), this))
    , valueSpin_(new QDoubleSpinBox(this))
    , modeLabel_(new QLabel(tr("Режим:"), this))
    , modeCombo_(new QComboBox(this))
    , errorLabel_(new QLabel(this))
    , createButton_(new QPushButton(tr("Создать"), this))
    , cancelButton_(new QPushButton(tr("Отмена"), this)) {
    setWindowTitle(tr("Добавить правило автоматизации"));
    setModal(true);
    setMinimumWidth(440);

    idEdit_->setObjectName(QStringLiteral("addRuleIdEdit"));
    idEdit_->setPlaceholderText(tr("Например: evening_lamp_on"));

    typeCombo_->setObjectName(QStringLiteral("addRuleTypeCombo"));
    typeCombo_->addItems({
        QStringLiteral("point"),
        QStringLiteral("time_window"),
        QStringLiteral("periodic"),
        QStringLiteral("timestamp"),
    });

    enabledCheck_->setChecked(true);

    // Page 0: point
    auto *pointPage = new QWidget(this);
    auto *pointForm = new QFormLayout(pointPage);
    pointForm->setContentsMargins(0, 0, 0, 0);
    pointTimeEdit_->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    repeatDailyCheck_->setChecked(true);
    pointForm->addRow(tr("Время (HH:MM:SS):"), pointTimeEdit_);
    pointForm->addRow(QString(), repeatDailyCheck_);
    scheduleStack_->addWidget(pointPage);

    // Page 1: time_window
    auto *windowPage = new QWidget(this);
    auto *windowForm = new QFormLayout(windowPage);
    windowForm->setContentsMargins(0, 0, 0, 0);
    startTimeEdit_->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    endTimeEdit_->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    windowRepeatDailyCheck_->setChecked(true);
    windowForm->addRow(tr("Начало окна:"), startTimeEdit_);
    windowForm->addRow(tr("Конец окна:"), endTimeEdit_);
    windowForm->addRow(QString(), windowRepeatDailyCheck_);
    scheduleStack_->addWidget(windowPage);

    // Page 2: periodic
    auto *periodicPage = new QWidget(this);
    auto *periodicForm = new QFormLayout(periodicPage);
    periodicForm->setContentsMargins(0, 0, 0, 0);
    intervalSecondsSpin_->setRange(1, 86400);
    intervalSecondsSpin_->setValue(60);
    intervalSecondsSpin_->setSuffix(tr(" сек"));
    maxTicksSpin_->setRange(0, 100000);
    maxTicksSpin_->setValue(0);
    maxTicksSpin_->setSpecialValueText(tr("Без лимита (0)"));
    periodicForm->addRow(tr("Интервал:"), intervalSecondsSpin_);
    periodicForm->addRow(tr("Макс. срабатываний:"), maxTicksSpin_);
    scheduleStack_->addWidget(periodicPage);

    // Page 3: timestamp
    auto *tsPage = new QWidget(this);
    auto *tsForm = new QFormLayout(tsPage);
    tsForm->setContentsMargins(0, 0, 0, 0);
    timestampEdit_->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    timestampEdit_->setCalendarPopup(true);
    tsForm->addRow(tr("Дата и время:"), timestampEdit_);
    scheduleStack_->addWidget(tsPage);

    deviceCombo_->setObjectName(QStringLiteral("addRuleDeviceCombo"));
    if (deviceNames.isEmpty()) {
        deviceCombo_->addItems({
            QStringLiteral("Lamp1"),
            QStringLiteral("Plug1"),
            QStringLiteral("Rad1"),
            QStringLiteral("Therm1"),
            QStringLiteral("Hygro1"),
        });
    } else {
        deviceCombo_->addItems(deviceNames);
    }

    commandCombo_->setObjectName(QStringLiteral("addRuleCommandCombo"));
    commandCombo_->addItems({
        QStringLiteral("ON"),
        QStringLiteral("OFF"),
        QStringLiteral("SET_BRIGHTNESS"),
        QStringLiteral("SET_VALVE_LEVEL"),
        QStringLiteral("SET_TEMPERATURE"),
        QStringLiteral("SET_MODE"),
    });

    valueSpin_->setRange(0.0, 100.0);
    valueSpin_->setDecimals(1);
    valueSpin_->setValue(50.0);

    modeCombo_->addItems({QStringLiteral("HEAT"), QStringLiteral("COOL"), QStringLiteral("OFF")});

    errorLabel_->setObjectName(QStringLiteral("addRuleErrorLabel"));
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();

    createButton_->setObjectName(QStringLiteral("addRuleCreateButton"));
    createButton_->setDefault(true);

    auto *form = new QFormLayout();
    form->setSpacing(8);
    form->addRow(tr("ID правила*:"), idEdit_);
    form->addRow(tr("Тип таймера:"), typeCombo_);
    form->addRow(QString(), enabledCheck_);
    form->addRow(tr("Расписание:"), scheduleStack_);
    form->addRow(tr("Устройство:"), deviceCombo_);
    form->addRow(tr("Команда:"), commandCombo_);
    form->addRow(valueLabel_, valueSpin_);
    form->addRow(modeLabel_, modeCombo_);

    auto *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch(1);
    buttonsLayout->addWidget(cancelButton_);
    buttonsLayout->addWidget(createButton_);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);
    rootLayout->addLayout(form);
    rootLayout->addWidget(errorLabel_);
    rootLayout->addLayout(buttonsLayout);

    connect(typeCombo_, &QComboBox::currentTextChanged, this, &AddRuleDialog::onTypeChanged);
    connect(commandCombo_, &QComboBox::currentTextChanged, this, &AddRuleDialog::onCommandChanged);
    connect(createButton_, &QPushButton::clicked, this, &AddRuleDialog::onSubmitClicked);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);

    onTypeChanged(typeCombo_->currentText());
    onCommandChanged(commandCombo_->currentText());
}

void AddRuleDialog::onTypeChanged(const QString &type) {
    if (type == QStringLiteral("point")) {
        scheduleStack_->setCurrentIndex(0);
    } else if (type == QStringLiteral("time_window")) {
        scheduleStack_->setCurrentIndex(1);
    } else if (type == QStringLiteral("periodic")) {
        scheduleStack_->setCurrentIndex(2);
    } else {
        scheduleStack_->setCurrentIndex(3);
    }
}

void AddRuleDialog::onCommandChanged(const QString &command) {
    const bool needsValue = (command == QStringLiteral("SET_BRIGHTNESS")
                             || command == QStringLiteral("SET_VALVE_LEVEL")
                             || command == QStringLiteral("SET_TEMPERATURE"));
    const bool needsMode = (command == QStringLiteral("SET_MODE"));

    valueLabel_->setVisible(needsValue);
    valueSpin_->setVisible(needsValue);
    modeLabel_->setVisible(needsMode);
    modeCombo_->setVisible(needsMode);

    if (command == QStringLiteral("SET_TEMPERATURE")) {
        valueSpin_->setRange(10.0, 35.0);
        valueSpin_->setSingleStep(0.5);
        valueSpin_->setValue(22.0);
    } else if (needsValue) {
        valueSpin_->setRange(0.0, 100.0);
        valueSpin_->setSingleStep(5.0);
        valueSpin_->setValue(50.0);
    }
}

QJsonObject AddRuleDialog::buildActionCommandJson() const {
    const QString cmd = commandCombo_->currentText();
    QJsonObject obj;
    obj.insert(QStringLiteral("command"), cmd);
    if (cmd == QStringLiteral("SET_MODE")) {
        obj.insert(QStringLiteral("mode"), modeCombo_->currentText());
    } else if (cmd == QStringLiteral("SET_BRIGHTNESS")
               || cmd == QStringLiteral("SET_VALVE_LEVEL")) {
        obj.insert(QStringLiteral("value"), static_cast<int>(valueSpin_->value()));
    } else if (cmd == QStringLiteral("SET_TEMPERATURE")) {
        obj.insert(QStringLiteral("value"), valueSpin_->value());
    }
    return obj;
}

QJsonObject AddRuleDialog::buildEndActionCommandJson() const {
    QJsonObject obj;
    if (deviceCombo_->currentText().startsWith(QStringLiteral("Therm"))) {
        obj.insert(QStringLiteral("command"), QStringLiteral("SET_MODE"));
        obj.insert(QStringLiteral("mode"), QStringLiteral("OFF"));
    } else {
        obj.insert(QStringLiteral("command"), QStringLiteral("OFF"));
    }
    return obj;
}

smarthome::net::RuleItem AddRuleDialog::buildRuleItem() const {
    smarthome::net::RuleItem rule;
    rule.id = idEdit_->text().trimmed();
    rule.type = typeCombo_->currentText();
    rule.enabled = enabledCheck_->isChecked();

    smarthome::net::RuleAction mainAction;
    mainAction.device = deviceCombo_->currentText().trimmed();
    mainAction.command = buildActionCommandJson();

    if (rule.type == QStringLiteral("point")) {
        rule.time = pointTimeEdit_->time().toString(QStringLiteral("HH:mm:ss"));
        rule.repeatDaily = repeatDailyCheck_->isChecked();
        rule.action = mainAction;
    } else if (rule.type == QStringLiteral("time_window")) {
        rule.startTime = startTimeEdit_->time().toString(QStringLiteral("HH:mm:ss"));
        rule.endTime = endTimeEdit_->time().toString(QStringLiteral("HH:mm:ss"));
        rule.repeatDaily = windowRepeatDailyCheck_->isChecked();
        rule.startAction = mainAction;
        rule.action = mainAction;

        smarthome::net::RuleAction endAct;
        endAct.device = mainAction.device;
        endAct.command = buildEndActionCommandJson();
        rule.endAction = endAct;
    } else if (rule.type == QStringLiteral("periodic")) {
        rule.intervalSeconds = intervalSecondsSpin_->value();
        rule.maxTicks = maxTicksSpin_->value();
        rule.action = mainAction;
    } else if (rule.type == QStringLiteral("timestamp")) {
        rule.timestamp = timestampEdit_->dateTime().toSecsSinceEpoch();
        rule.action = mainAction;
    }

    return rule;
}

void AddRuleDialog::onSubmitClicked() {
    const QString id = idEdit_->text().trimmed();
    if (id.isEmpty()) {
        showInlineError(tr("Введите уникальный идентификатор правила (ID)."));
        idEdit_->setFocus();
        return;
    }

    errorLabel_->hide();
    createButton_->setEnabled(false);
    emit ruleSubmitted(buildRuleItem());
}

void AddRuleDialog::handleSubmitSuccess() {
    createButton_->setEnabled(true);
    accept();
}

void AddRuleDialog::handleSubmitError(int httpStatus, const QString &message) {
    createButton_->setEnabled(true);
    if (httpStatus == 409) {
        showInlineError(tr("Конфликт (409): правило с ID '%1' уже существует. Укажите другой ID.")
                            .arg(idEdit_->text().trimmed()));
        idEdit_->setFocus();
        idEdit_->selectAll();
        return;
    }
    showInlineError(tr("Не удалось создать правило (%1): %2").arg(httpStatus).arg(message));
}

void AddRuleDialog::showInlineError(const QString &message) {
    errorLabel_->setText(message);
    errorLabel_->show();
}

} // namespace smarthome::ui
