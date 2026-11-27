#include "widgets/DeviceCardWidget.h"

#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace smarthome::ui {

DeviceCardWidget::DeviceCardWidget(const QString &deviceName,
                                   const QString &deviceType,
                                   QWidget *parent)
    : QFrame(parent)
    , deviceName_(deviceName)
    , deviceType_(deviceType)
    , nameLabel_(new QLabel(deviceName, this))
    , typeLabel_(new QLabel(formatDeviceTypeTitle(deviceType), this))
    , onlineBadgeLabel_(new QLabel(this))
    , updatedAtLabel_(new QLabel(tr("Обновление: нет данных"), this))
    , metricsLayout_(new QGridLayout())
    , controlsLayout_(new QVBoxLayout())
    , sliderDebounceTimer_(new QTimer(this))
    , tempDebounceTimer_(new QTimer(this)) {
    setObjectName(QStringLiteral("deviceCard"));
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(280);

    nameLabel_->setObjectName(QStringLiteral("deviceCardName"));
    typeLabel_->setObjectName(QStringLiteral("deviceCardType"));
    onlineBadgeLabel_->setObjectName(QStringLiteral("onlineBadge"));
    onlineBadgeLabel_->setAlignment(Qt::AlignCenter);
    updatedAtLabel_->setObjectName(QStringLiteral("deviceCardUpdatedAt"));

    sliderDebounceTimer_->setSingleShot(true);
    tempDebounceTimer_->setSingleShot(true);
    connect(sliderDebounceTimer_, &QTimer::timeout, this, &DeviceCardWidget::dispatchLevelSliderCommand);
    connect(tempDebounceTimer_, &QTimer::timeout, this, &DeviceCardWidget::dispatchTargetTempCommand);

    auto *headerLayout = new QHBoxLayout();
    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->addWidget(nameLabel_);
    titleLayout->addWidget(typeLabel_);

    headerLayout->addLayout(titleLayout, 1);
    headerLayout->addWidget(onlineBadgeLabel_, 0, Qt::AlignTop);

    metricsLayout_->setContentsMargins(0, 6, 0, 4);
    metricsLayout_->setHorizontalSpacing(12);
    metricsLayout_->setVerticalSpacing(4);
    metricsLayout_->setColumnStretch(0, 1);
    metricsLayout_->setColumnStretch(1, 0);

    controlsLayout_->setContentsMargins(0, 6, 0, 2);
    controlsLayout_->setSpacing(8);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 12, 14, 12);
    rootLayout->setSpacing(8);
    rootLayout->addLayout(headerLayout);
    rootLayout->addLayout(metricsLayout_);
    rootLayout->addLayout(controlsLayout_);
    rootLayout->addWidget(updatedAtLabel_);
    rootLayout->addStretch(1);

    setOnlineState(false);
    rebuildControlsIfNeeded();
}

void DeviceCardWidget::setDebounceIntervalMs(int intervalMs) {
    debounceIntervalMs_ = qMax(20, intervalMs);
}

QString DeviceCardWidget::onlineBadgeText() const {
    return onlineBadgeLabel_->text();
}

QString DeviceCardWidget::lastUpdatedText() const {
    return updatedAtLabel_->text();
}

QString DeviceCardWidget::metricsText() const {
    QStringList lines;
    lines.reserve(currentMetrics_.size());
    for (const auto &entry : currentMetrics_) {
        lines.append(QStringLiteral("%1: %2").arg(entry.first, entry.second));
    }
    return lines.join(QLatin1Char('\n'));
}

void DeviceCardWidget::updateFromStatus(const smarthome::net::DeviceStatus &status) {
    if (!status.deviceName.isEmpty()) {
        deviceName_ = status.deviceName;
        nameLabel_->setText(deviceName_);
    }
    if (!status.deviceType.isEmpty()) {
        deviceType_ = status.deviceType;
        typeLabel_->setText(formatDeviceTypeTitle(deviceType_));
    }

    setOnlineState(status.isOnline);
    rebuildControlsIfNeeded();

    if (status.telemetry.has_value()) {
        updateTimestampLabel(*status.telemetry);
        rebuildMetrics(*status.telemetry);
        syncControlsFromTelemetry(*status.telemetry);
    } else {
        updatedAtLabel_->setText(tr("Обновление: нет данных"));
        clearLayout(metricsLayout_);
        currentMetrics_.clear();
        auto *emptyLabel = new QLabel(tr("Ожидание телеметрии..."), this);
        emptyLabel->setObjectName(QStringLiteral("metricValue"));
        metricsLayout_->addWidget(emptyLabel, 0, 0, 1, 2);
    }
}

void DeviceCardWidget::applyLiveTelemetry(const smarthome::net::DeviceTelemetry &telemetry) {
    if (!telemetry.deviceType.isEmpty() && telemetry.deviceType != QStringLiteral("Unknown")) {
        deviceType_ = telemetry.deviceType;
        typeLabel_->setText(formatDeviceTypeTitle(deviceType_));
    }
    setOnlineState(true);
    rebuildControlsIfNeeded();
    updateTimestampLabel(telemetry);
    rebuildMetrics(telemetry);
    syncControlsFromTelemetry(telemetry);
}

void DeviceCardWidget::setOnlineState(bool online) {
    isOnline_ = online;
    onlineBadgeLabel_->setText(online ? QStringLiteral("Online") : QStringLiteral("Offline"));
    onlineBadgeLabel_->setProperty("online", online);
    onlineBadgeLabel_->style()->unpolish(onlineBadgeLabel_);
    onlineBadgeLabel_->style()->polish(onlineBadgeLabel_);
    onlineBadgeLabel_->update();
}

void DeviceCardWidget::updateTimestampLabel(const smarthome::net::DeviceTelemetry &telemetry) {
    const QString formatted = formatTimestamp(telemetry);
    if (formatted.isEmpty()) {
        updatedAtLabel_->setText(tr("Обновление: нет данных"));
    } else {
        updatedAtLabel_->setText(tr("Обновление: %1").arg(formatted));
    }
}

void DeviceCardWidget::rebuildMetrics(const smarthome::net::DeviceTelemetry &telemetry) {
    clearLayout(metricsLayout_);
    currentMetrics_ = extractProfileMetrics(deviceType_, telemetry);

    int row = 0;
    for (const auto &metric : currentMetrics_) {
        auto *keyLabel = new QLabel(metric.first, this);
        keyLabel->setObjectName(QStringLiteral("metricKey"));

        auto *valLabel = new QLabel(metric.second, this);
        valLabel->setObjectName(QStringLiteral("metricValue"));
        valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        metricsLayout_->addWidget(keyLabel, row, 0);
        metricsLayout_->addWidget(valLabel, row, 1);
        ++row;
    }
}

void DeviceCardWidget::rebuildControlsIfNeeded() {
    if (controlsBuiltForType_ == deviceType_) {
        return;
    }
    controlsBuiltForType_ = deviceType_;

    sliderDebounceTimer_->stop();
    tempDebounceTimer_->stop();
    clearLayout(controlsLayout_);
    onButton_ = nullptr;
    offButton_ = nullptr;
    levelSlider_ = nullptr;
    levelValueLabel_ = nullptr;
    levelCommandName_.clear();
    targetTempSpinBox_ = nullptr;
    modeComboBox_ = nullptr;

    if (!isActuatorType(deviceType_)) {
        return;
    }

    auto *divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("cardDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    controlsLayout_->addWidget(divider);

    addPowerButtons();

    if (deviceType_ == QStringLiteral("Lamp")) {
        addLevelSliderControl(tr("Яркость"), QStringLiteral("SET_BRIGHTNESS"));
    } else if (deviceType_ == QStringLiteral("Radiator")) {
        addLevelSliderControl(tr("Клапан"), QStringLiteral("SET_VALVE_LEVEL"));
    } else if (deviceType_ == QStringLiteral("Thermostat")) {
        addThermostatControls();
    }
}

void DeviceCardWidget::addPowerButtons() {
    onButton_ = new QPushButton(tr("ВКЛ"), this);
    onButton_->setObjectName(QStringLiteral("powerOnButton"));

    offButton_ = new QPushButton(tr("ВЫКЛ"), this);
    offButton_->setObjectName(QStringLiteral("powerOffButton"));

    connect(onButton_, &QPushButton::clicked, this, [this]() {
        if (deviceType_ == QStringLiteral("Thermostat")) {
            emit commandRequested(deviceName_, smarthome::net::CommandRequest{
                QStringLiteral("SET_MODE"), std::nullopt, QStringLiteral("HEAT")});
        } else {
            emit commandRequested(deviceName_, smarthome::net::CommandRequest{
                QStringLiteral("ON"), std::nullopt, std::nullopt});
        }
    });
    connect(offButton_, &QPushButton::clicked, this, [this]() {
        if (deviceType_ == QStringLiteral("Thermostat")) {
            emit commandRequested(deviceName_, smarthome::net::CommandRequest{
                QStringLiteral("SET_MODE"), std::nullopt, QStringLiteral("OFF")});
        } else {
            emit commandRequested(deviceName_, smarthome::net::CommandRequest{
                QStringLiteral("OFF"), std::nullopt, std::nullopt});
        }
    });

    auto *row = new QHBoxLayout();
    row->setSpacing(8);
    row->addWidget(onButton_);
    row->addWidget(offButton_);
    controlsLayout_->addLayout(row);
}

void DeviceCardWidget::addLevelSliderControl(const QString &labelText, const QString &commandName) {
    levelCommandName_ = commandName;

    auto *titleLabel = new QLabel(labelText, this);
    titleLabel->setObjectName(QStringLiteral("controlLabel"));

    levelValueLabel_ = new QLabel(QStringLiteral("0 %"), this);
    levelValueLabel_->setObjectName(QStringLiteral("controlValueLabel"));
    levelValueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *header = new QHBoxLayout();
    header->addWidget(titleLabel);
    header->addStretch(1);
    header->addWidget(levelValueLabel_);

    levelSlider_ = new QSlider(Qt::Horizontal, this);
    levelSlider_->setObjectName(QStringLiteral("deviceLevelSlider"));
    levelSlider_->setRange(0, 100);
    levelSlider_->setSingleStep(5);
    levelSlider_->setPageStep(10);

    connect(levelSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (levelValueLabel_ != nullptr) {
            levelValueLabel_->setText(QStringLiteral("%1 %").arg(value));
        }
        if (updatingControls_) {
            return;
        }
        if (!levelSlider_->isSliderDown()) {
            sliderDebounceTimer_->start(debounceIntervalMs_);
        }
    });

    connect(levelSlider_, &QSlider::sliderReleased, this, [this]() {
        if (updatingControls_) {
            return;
        }
        sliderDebounceTimer_->stop();
        dispatchLevelSliderCommand();
    });

    controlsLayout_->addLayout(header);
    controlsLayout_->addWidget(levelSlider_);
}

void DeviceCardWidget::addThermostatControls() {
    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    auto *tempLabel = new QLabel(tr("Цель, °C"), this);
    tempLabel->setObjectName(QStringLiteral("controlLabel"));

    targetTempSpinBox_ = new QDoubleSpinBox(this);
    targetTempSpinBox_->setObjectName(QStringLiteral("targetTempSpinBox"));
    targetTempSpinBox_->setRange(10.0, 35.0);
    targetTempSpinBox_->setSingleStep(0.5);
    targetTempSpinBox_->setDecimals(1);
    targetTempSpinBox_->setSuffix(QStringLiteral(" °C"));

    connect(targetTempSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (updatingControls_) {
            return;
        }
        tempDebounceTimer_->start(debounceIntervalMs_);
    });

    auto *modeLabel = new QLabel(tr("Режим"), this);
    modeLabel->setObjectName(QStringLiteral("controlLabel"));

    modeComboBox_ = new QComboBox(this);
    modeComboBox_->setObjectName(QStringLiteral("thermostatModeCombo"));
    modeComboBox_->addItems({QStringLiteral("OFF"), QStringLiteral("COOL"), QStringLiteral("HEAT")});

    connect(modeComboBox_, &QComboBox::currentTextChanged, this, [this](const QString &mode) {
        if (updatingControls_ || mode.isEmpty()) {
            return;
        }
        emit commandRequested(deviceName_, smarthome::net::CommandRequest{
            QStringLiteral("SET_MODE"), std::nullopt, mode});
    });

    grid->addWidget(tempLabel, 0, 0);
    grid->addWidget(targetTempSpinBox_, 0, 1);
    grid->addWidget(modeLabel, 1, 0);
    grid->addWidget(modeComboBox_, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    controlsLayout_->addLayout(grid);
}

void DeviceCardWidget::syncControlsFromTelemetry(const smarthome::net::DeviceTelemetry &telemetry) {
    updatingControls_ = true;

    if (onButton_ != nullptr && offButton_ != nullptr) {
        const bool isActive = (deviceType_ == QStringLiteral("Thermostat"))
            ? (telemetry.mode.trimmed().toUpper() != QStringLiteral("OFF"))
            : telemetry.isOn;
        onButton_->setProperty("activePower", isActive);
        offButton_->setProperty("activePower", !isActive);
        onButton_->style()->unpolish(onButton_);
        onButton_->style()->polish(onButton_);
        offButton_->style()->unpolish(offButton_);
        offButton_->style()->polish(offButton_);
    }

    if (levelSlider_ != nullptr && !levelSlider_->isSliderDown() && !sliderDebounceTimer_->isActive()) {
        const int targetLevel = (deviceType_ == QStringLiteral("Lamp"))
            ? telemetry.brightness
            : telemetry.currentValveLevel;
        levelSlider_->setValue(qBound(0, targetLevel, 100));
        if (levelValueLabel_ != nullptr) {
            levelValueLabel_->setText(QStringLiteral("%1 %").arg(levelSlider_->value()));
        }
    }

    if (targetTempSpinBox_ != nullptr && !targetTempSpinBox_->hasFocus() && !tempDebounceTimer_->isActive()) {
        targetTempSpinBox_->setValue(qBound(10.0, telemetry.targetTemperature, 35.0));
    }

    if (modeComboBox_ != nullptr && !modeComboBox_->hasFocus()) {
        const QString normalizedMode = telemetry.mode.trimmed().toUpper();
        const int idx = modeComboBox_->findText(normalizedMode);
        if (idx >= 0) {
            modeComboBox_->setCurrentIndex(idx);
        }
    }

    updatingControls_ = false;
}

void DeviceCardWidget::dispatchLevelSliderCommand() {
    if (levelSlider_ == nullptr || levelCommandName_.isEmpty()) {
        return;
    }
    emit commandRequested(deviceName_, smarthome::net::CommandRequest{
        levelCommandName_, static_cast<double>(levelSlider_->value()), std::nullopt});
}

void DeviceCardWidget::dispatchTargetTempCommand() {
    if (targetTempSpinBox_ == nullptr) {
        return;
    }
    emit commandRequested(deviceName_, smarthome::net::CommandRequest{
        QStringLiteral("SET_TEMPERATURE"), targetTempSpinBox_->value(), std::nullopt});
}

void DeviceCardWidget::clearLayout(QLayout *layout) {
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QLayout *childLayout = item->layout()) {
            clearLayout(childLayout);
        }
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

bool DeviceCardWidget::isActuatorType(const QString &deviceType) {
    return deviceType == QStringLiteral("Lamp")
        || deviceType == QStringLiteral("Plug")
        || deviceType == QStringLiteral("Radiator")
        || deviceType == QStringLiteral("Thermostat");
}

QVector<QPair<QString, QString>> DeviceCardWidget::extractProfileMetrics(
    const QString &deviceType,
    const smarthome::net::DeviceTelemetry &telemetry) {
    const QString powerState = telemetry.isOn ? QStringLiteral("ВКЛ") : QStringLiteral("ВЫКЛ");

    if (deviceType == QStringLiteral("Lamp")) {
        return {
            {QStringLiteral("Состояние"), powerState},
            {QStringLiteral("Яркость"), QStringLiteral("%1 %").arg(telemetry.brightness)},
        };
    }
    if (deviceType == QStringLiteral("Plug")) {
        return {
            {QStringLiteral("Состояние"), powerState},
            {QStringLiteral("Мощность"), QStringLiteral("%1 Вт").arg(telemetry.currentPower, 0, 'f', 1)},
        };
    }
    if (deviceType == QStringLiteral("Radiator")) {
        return {
            {QStringLiteral("Состояние"), powerState},
            {QStringLiteral("Клапан"), QStringLiteral("%1 %").arg(telemetry.currentValveLevel)},
            {QStringLiteral("Температура"), QStringLiteral("%1 °C").arg(telemetry.currentTemperature, 0, 'f', 1)},
        };
    }
    if (deviceType == QStringLiteral("Thermostat")) {
        return {
            {QStringLiteral("Текущая темп."), QStringLiteral("%1 °C").arg(telemetry.currentTemperature, 0, 'f', 1)},
            {QStringLiteral("Целевая темп."), QStringLiteral("%1 °C").arg(telemetry.targetTemperature, 0, 'f', 1)},
            {QStringLiteral("Режим"), telemetry.mode.isEmpty() ? QStringLiteral("OFF") : telemetry.mode},
        };
    }
    if (deviceType == QStringLiteral("Hygrometer")) {
        return {
            {QStringLiteral("Влажность"), QStringLiteral("%1 %").arg(telemetry.currentHumidity, 0, 'f', 1)},
            {QStringLiteral("Температура"), QStringLiteral("%1 °C").arg(telemetry.currentTemperature, 0, 'f', 1)},
        };
    }

    return {
        {QStringLiteral("Состояние"), powerState},
    };
}

QString DeviceCardWidget::formatTimestamp(const smarthome::net::DeviceTelemetry &telemetry) {
    if (!telemetry.timestampIso.trimmed().isEmpty()) {
        const QDateTime dt = QDateTime::fromString(telemetry.timestampIso.trimmed(), Qt::ISODateWithMs);
        if (dt.isValid()) {
            return dt.toLocalTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
        }
        return telemetry.timestampIso.trimmed();
    }
    if (telemetry.timestamp > 0) {
        return QDateTime::fromMSecsSinceEpoch(telemetry.timestamp)
            .toLocalTime()
            .toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    }
    return {};
}

QString DeviceCardWidget::formatDeviceTypeTitle(const QString &deviceType) {
    if (deviceType == QStringLiteral("Lamp")) {
        return QStringLiteral("Умная лампа (Lamp)");
    }
    if (deviceType == QStringLiteral("Plug")) {
        return QStringLiteral("Умная розетка (Plug)");
    }
    if (deviceType == QStringLiteral("Radiator")) {
        return QStringLiteral("Радиатор (Radiator)");
    }
    if (deviceType == QStringLiteral("Thermostat")) {
        return QStringLiteral("Термостат (Thermostat)");
    }
    if (deviceType == QStringLiteral("Hygrometer")) {
        return QStringLiteral("Гигрометр (Hygrometer)");
    }
    return deviceType;
}

} // namespace smarthome::ui
