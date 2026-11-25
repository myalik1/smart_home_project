#include "widgets/DeviceCardWidget.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QStyle>
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
    , metricsLayout_(new QGridLayout()) {
    setObjectName(QStringLiteral("deviceCard"));
    setFrameShape(QFrame::StyledPanel);
    setMinimumWidth(270);

    nameLabel_->setObjectName(QStringLiteral("deviceCardName"));
    typeLabel_->setObjectName(QStringLiteral("deviceCardType"));
    onlineBadgeLabel_->setObjectName(QStringLiteral("onlineBadge"));
    onlineBadgeLabel_->setAlignment(Qt::AlignCenter);
    updatedAtLabel_->setObjectName(QStringLiteral("deviceCardUpdatedAt"));

    auto *headerLayout = new QHBoxLayout();
    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    titleLayout->addWidget(nameLabel_);
    titleLayout->addWidget(typeLabel_);

    headerLayout->addLayout(titleLayout, 1);
    headerLayout->addWidget(onlineBadgeLabel_, 0, Qt::AlignTop);

    metricsLayout_->setContentsMargins(0, 6, 0, 6);
    metricsLayout_->setHorizontalSpacing(12);
    metricsLayout_->setVerticalSpacing(4);
    metricsLayout_->setColumnStretch(0, 1);
    metricsLayout_->setColumnStretch(1, 0);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 12, 14, 12);
    rootLayout->setSpacing(8);
    rootLayout->addLayout(headerLayout);
    rootLayout->addLayout(metricsLayout_);
    rootLayout->addWidget(updatedAtLabel_);
    rootLayout->addStretch(1);

    setOnlineState(false);
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

    if (status.telemetry.has_value()) {
        updateTimestampLabel(*status.telemetry);
        rebuildMetrics(*status.telemetry);
    } else {
        updatedAtLabel_->setText(tr("Обновление: нет данных"));
        clearMetricsLayout();
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
    updateTimestampLabel(telemetry);
    rebuildMetrics(telemetry);
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
    clearMetricsLayout();
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

void DeviceCardWidget::clearMetricsLayout() {
    while (QLayoutItem *item = metricsLayout_->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
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
