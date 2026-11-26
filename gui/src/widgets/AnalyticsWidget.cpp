#include "widgets/AnalyticsWidget.h"

#include <QAbstractItemView>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineSeries>
#include <QPen>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <limits>

namespace smarthome::ui {

namespace {
constexpr int kHistoryChartLimit = 1000;
constexpr int kHistoryTableLimit = 100;
constexpr int kPreset15Min = 15;
constexpr int kPreset1Hour = 60;
constexpr int kPreset24Hours = 24 * 60;
constexpr int kPresetCustom = -1;

struct SeriesSpec {
    QString yAxisTitle;
    QString primaryLabel;
    QString secondaryLabel;
    bool hasSecondary = false;
};

SeriesSpec chartSpecForType(const QString &deviceType) {
    if (deviceType == QStringLiteral("Thermostat")) {
        return {QStringLiteral("Температура (°C)"),
                QStringLiteral("Текущая температура (°C)"),
                QStringLiteral("Целевая температура (°C)"),
                true};
    }
    if (deviceType == QStringLiteral("Hygrometer")) {
        return {QStringLiteral("Влажность (%) / Температура (°C)"),
                QStringLiteral("Влажность (%)"),
                QStringLiteral("Температура (°C)"),
                true};
    }
    if (deviceType == QStringLiteral("Plug")) {
        return {QStringLiteral("Мощность (Вт)"),
                QStringLiteral("Потребляемая мощность (Вт)"),
                {},
                false};
    }
    if (deviceType == QStringLiteral("Radiator")) {
        return {QStringLiteral("Клапан (%) / Температура (°C)"),
                QStringLiteral("Уровень клапана (%)"),
                QStringLiteral("Температура (°C)"),
                true};
    }
    if (deviceType == QStringLiteral("Lamp")) {
        return {QStringLiteral("Яркость (%)"),
                QStringLiteral("Яркость (%)"),
                {},
                false};
    }
    return {QStringLiteral("Значение"), QStringLiteral("Значение"), {}, false};
}

double extractPrimaryMetric(const QString &deviceType, const smarthome::net::DeviceTelemetry &t) {
    if (deviceType == QStringLiteral("Thermostat")) {
        return t.currentTemperature;
    }
    if (deviceType == QStringLiteral("Hygrometer")) {
        return t.currentHumidity;
    }
    if (deviceType == QStringLiteral("Plug")) {
        return t.currentPower;
    }
    if (deviceType == QStringLiteral("Radiator")) {
        return static_cast<double>(t.currentValveLevel);
    }
    if (deviceType == QStringLiteral("Lamp")) {
        return static_cast<double>(t.brightness);
    }
    return t.isOn ? 1.0 : 0.0;
}

double extractSecondaryMetric(const QString &deviceType, const smarthome::net::DeviceTelemetry &t) {
    if (deviceType == QStringLiteral("Thermostat")) {
        return t.targetTemperature;
    }
    if (deviceType == QStringLiteral("Hygrometer") || deviceType == QStringLiteral("Radiator")) {
        return t.currentTemperature;
    }
    return 0.0;
}
} // namespace

AnalyticsWidget::AnalyticsWidget(QWidget *parent)
    : QWidget(parent)
    , deviceCombo_(new QComboBox(this))
    , presetCombo_(new QComboBox(this))
    , startDateTimeEdit_(new QDateTimeEdit(this))
    , endDateTimeEdit_(new QDateTimeEdit(this))
    , refreshButton_(new QPushButton(tr("Обновить"), this))
    , chartStatusLabel_(new QLabel(tr("Выберите устройство и интервал"), this))
    , chart_(new QChart())
    , chartView_(new QChartView(chart_, this))
    , eventsTable_(new QTableWidget(this)) {
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    setupFilterBar(rootLayout);
    setupChartAndTable(rootLayout);

    applyPresetRange(kPreset1Hour);
}

void AnalyticsWidget::setupFilterBar(QVBoxLayout *rootLayout) {
    deviceCombo_->setObjectName(QStringLiteral("analyticsDeviceCombo"));
    deviceCombo_->addItems({
        QStringLiteral("Therm1"),
        QStringLiteral("Hygro1"),
        QStringLiteral("Plug1"),
        QStringLiteral("Rad1"),
        QStringLiteral("Lamp1"),
    });

    presetCombo_->setObjectName(QStringLiteral("analyticsPresetCombo"));
    presetCombo_->addItem(tr("Последние 15 мин"), kPreset15Min);
    presetCombo_->addItem(tr("Последний 1 час"), kPreset1Hour);
    presetCombo_->addItem(tr("Последние 24 часа"), kPreset24Hours);
    presetCombo_->addItem(tr("Произвольный интервал"), kPresetCustom);
    presetCombo_->setCurrentIndex(1);

    startDateTimeEdit_->setObjectName(QStringLiteral("analyticsStartDateTime"));
    startDateTimeEdit_->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    startDateTimeEdit_->setCalendarPopup(true);

    endDateTimeEdit_->setObjectName(QStringLiteral("analyticsEndDateTime"));
    endDateTimeEdit_->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    endDateTimeEdit_->setCalendarPopup(true);

    refreshButton_->setObjectName(QStringLiteral("analyticsRefreshButton"));
    chartStatusLabel_->setObjectName(QStringLiteral("analyticsChartStatus"));

    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);
    filterLayout->addWidget(new QLabel(tr("Устройство:"), this));
    filterLayout->addWidget(deviceCombo_);
    filterLayout->addWidget(new QLabel(tr("Интервал:"), this));
    filterLayout->addWidget(presetCombo_);
    filterLayout->addWidget(new QLabel(tr("С:"), this));
    filterLayout->addWidget(startDateTimeEdit_);
    filterLayout->addWidget(new QLabel(tr("По:"), this));
    filterLayout->addWidget(endDateTimeEdit_);
    filterLayout->addStretch(1);
    filterLayout->addWidget(refreshButton_);

    rootLayout->addLayout(filterLayout);
    rootLayout->addWidget(chartStatusLabel_);

    connect(presetCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (updatingRange_) {
            return;
        }
        const int minutes = presetCombo_->itemData(idx).toInt();
        if (minutes > 0) {
            applyPresetRange(minutes);
            requestRefresh();
        }
    });

    auto onManualDateChanged = [this]() {
        if (updatingRange_) {
            return;
        }
        updatingRange_ = true;
        const int customIdx = presetCombo_->findData(kPresetCustom);
        if (customIdx >= 0) {
            presetCombo_->setCurrentIndex(customIdx);
        }
        updatingRange_ = false;
    };
    connect(startDateTimeEdit_, &QDateTimeEdit::dateTimeChanged, this, onManualDateChanged);
    connect(endDateTimeEdit_, &QDateTimeEdit::dateTimeChanged, this, onManualDateChanged);

    connect(deviceCombo_, &QComboBox::currentTextChanged, this, [this](const QString &) {
        requestRefresh();
    });
    connect(refreshButton_, &QPushButton::clicked, this, &AnalyticsWidget::requestRefresh);
}

void AnalyticsWidget::setupChartAndTable(QVBoxLayout *rootLayout) {
    chart_->setTitle(tr("История телеметрии"));
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignBottom);

    chartView_->setObjectName(QStringLiteral("analyticsChartView"));
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartView_->setMinimumHeight(250);

    eventsTable_->setObjectName(QStringLiteral("analyticsEventsTable"));
    eventsTable_->setColumnCount(5);
    eventsTable_->setHorizontalHeaderLabels({
        tr("Время"),
        tr("Устройство"),
        tr("Тип"),
        tr("Состояние"),
        tr("Показатели"),
    });
    eventsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    eventsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    eventsTable_->setAlternatingRowColors(true);
    eventsTable_->verticalHeader()->setVisible(false);
    eventsTable_->horizontalHeader()->setStretchLastSection(true);
    eventsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    eventsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    eventsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    eventsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    auto *tableContainer = new QWidget(this);
    auto *tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 4, 0, 0);
    tableLayout->setSpacing(6);
    auto *tableTitle = new QLabel(tr("Журнал последних событий"), tableContainer);
    tableTitle->setObjectName(QStringLiteral("analyticsTableTitle"));
    tableLayout->addWidget(tableTitle);
    tableLayout->addWidget(eventsTable_, 1);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(chartView_);
    splitter->addWidget(tableContainer);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    rootLayout->addWidget(splitter, 1);
}

QString AnalyticsWidget::selectedDeviceName() const {
    return deviceCombo_->currentText();
}

qint64 AnalyticsWidget::startTimestampMs() const {
    return startDateTimeEdit_->dateTime().toMSecsSinceEpoch();
}

qint64 AnalyticsWidget::endTimestampMs() const {
    return endDateTimeEdit_->dateTime().toMSecsSinceEpoch();
}

void AnalyticsWidget::applyPresetRange(int presetMinutes) {
    updatingRange_ = true;
    const QDateTime now = QDateTime::currentDateTime();
    endDateTimeEdit_->setDateTime(now);
    startDateTimeEdit_->setDateTime(now.addSecs(-static_cast<qint64>(presetMinutes) * 60));
    updatingRange_ = false;
}

void AnalyticsWidget::setAvailableDevices(const QVector<smarthome::net::DeviceStatus> &devices) {
    const QString previous = deviceCombo_->currentText();
    QStringList names;
    names.reserve(devices.size());
    for (const auto &dev : devices) {
        if (!dev.deviceName.isEmpty() && !names.contains(dev.deviceName)) {
            names.append(dev.deviceName);
        }
    }
    if (names.isEmpty()) {
        return;
    }

    const bool blocked = deviceCombo_->blockSignals(true);
    deviceCombo_->clear();
    deviceCombo_->addItems(names);
    const int prevIdx = deviceCombo_->findText(previous);
    if (prevIdx >= 0) {
        deviceCombo_->setCurrentIndex(prevIdx);
    }
    deviceCombo_->blockSignals(blocked);
}

void AnalyticsWidget::requestRefresh() {
    const QString deviceName = selectedDeviceName();
    if (deviceName.isEmpty()) {
        return;
    }

    const int presetMinutes = presetCombo_->currentData().toInt();
    if (presetMinutes > 0) {
        applyPresetRange(presetMinutes);
    }

    qint64 startTs = startTimestampMs();
    qint64 endTs = endTimestampMs();
    if (startTs > endTs) {
        startTs = endTs;
        updatingRange_ = true;
        startDateTimeEdit_->setDateTime(QDateTime::fromMSecsSinceEpoch(startTs));
        updatingRange_ = false;
    }

    emit deviceHistoryRequested(deviceName, startTs, endTs, kHistoryChartLimit);
    emit allHistoryRequested(kHistoryTableLimit);
}

void AnalyticsWidget::setDeviceHistory(const QString &deviceName,
                                       const QVector<smarthome::net::DeviceTelemetry> &history) {
    if (deviceName != selectedDeviceName()) {
        return;
    }

    chart_->removeAllSeries();
    const auto oldAxes = chart_->axes();
    for (QAbstractAxis *axis : oldAxes) {
        chart_->removeAxis(axis);
        delete axis;
    }

    const QString devType = inferDeviceType(deviceName, history);
    const SeriesSpec spec = chartSpecForType(devType);

    if (history.isEmpty()) {
        chart_->setTitle(tr("%1 — нет данных за выбранный интервал").arg(deviceName));
        chartStatusLabel_->setText(tr("Точек на графике: 0 (%1)").arg(deviceName));
        return;
    }

    auto *primarySeries = new QLineSeries(chart_);
    primarySeries->setName(spec.primaryLabel);
    QPen primaryPen(QColor(QStringLiteral("#2563eb")));
    primaryPen.setWidth(2);
    primarySeries->setPen(primaryPen);

    QLineSeries *secondarySeries = nullptr;
    if (spec.hasSecondary) {
        secondarySeries = new QLineSeries(chart_);
        secondarySeries->setName(spec.secondaryLabel);
        QPen secondaryPen(QColor(QStringLiteral("#ea580c")));
        secondaryPen.setWidth(2);
        secondaryPen.setStyle(Qt::DashLine);
        secondarySeries->setPen(secondaryPen);
    }

    qint64 minTs = std::numeric_limits<qint64>::max();
    qint64 maxTs = std::numeric_limits<qint64>::min();
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();

    for (const auto &point : history) {
        if (point.timestamp <= 0) {
            continue;
        }
        const qreal x = static_cast<qreal>(point.timestamp);
        const double y1 = extractPrimaryMetric(devType, point);
        primarySeries->append(x, y1);
        minTs = qMin(minTs, point.timestamp);
        maxTs = qMax(maxTs, point.timestamp);
        minVal = qMin(minVal, y1);
        maxVal = qMax(maxVal, y1);

        if (secondarySeries != nullptr) {
            const double y2 = extractSecondaryMetric(devType, point);
            secondarySeries->append(x, y2);
            minVal = qMin(minVal, y2);
            maxVal = qMax(maxVal, y2);
        }
    }

    if (primarySeries->count() == 0) {
        delete primarySeries;
        delete secondarySeries;
        chart_->setTitle(tr("%1 — нет корректных временных меток").arg(deviceName));
        chartStatusLabel_->setText(tr("Точек на графике: 0 (%1)").arg(deviceName));
        return;
    }

    chart_->addSeries(primarySeries);
    if (secondarySeries != nullptr) {
        chart_->addSeries(secondarySeries);
    }

    chart_->setTitle(tr("%1 (%2)").arg(deviceName, devType));
    chartStatusLabel_->setText(
        tr("Точек на графике: %1 (%2)").arg(primarySeries->count()).arg(deviceName));

    updateChartAxes(minTs, maxTs, minVal, maxVal, spec.yAxisTitle);
}

void AnalyticsWidget::updateChartAxes(qint64 minTsMs,
                                      qint64 maxTsMs,
                                      double minVal,
                                      double maxVal,
                                      const QString &yTitle) {
    auto *xAxis = new QDateTimeAxis(chart_);
    const qint64 spanMs = maxTsMs - minTsMs;
    xAxis->setFormat(spanMs > 12LL * 3600LL * 1000LL
                         ? QStringLiteral("dd.MM HH:mm")
                         : QStringLiteral("HH:mm:ss"));
    xAxis->setTitleText(tr("Время"));
    xAxis->setTickCount(6);
    if (minTsMs == maxTsMs) {
        xAxis->setRange(QDateTime::fromMSecsSinceEpoch(minTsMs - 60000),
                        QDateTime::fromMSecsSinceEpoch(maxTsMs + 60000));
    } else {
        xAxis->setRange(QDateTime::fromMSecsSinceEpoch(minTsMs),
                        QDateTime::fromMSecsSinceEpoch(maxTsMs));
    }

    auto *yAxis = new QValueAxis(chart_);
    yAxis->setTitleText(yTitle);
    if (qFuzzyCompare(minVal, maxVal)) {
        yAxis->setRange(minVal - 1.0, maxVal + 1.0);
    } else {
        const double pad = (maxVal - minVal) * 0.1;
        yAxis->setRange(minVal - pad, maxVal + pad);
    }

    chart_->addAxis(xAxis, Qt::AlignBottom);
    chart_->addAxis(yAxis, Qt::AlignLeft);
    for (QAbstractSeries *series : chart_->series()) {
        series->attachAxis(xAxis);
        series->attachAxis(yAxis);
    }
}

void AnalyticsWidget::setAllHistory(const QVector<smarthome::net::DeviceTelemetry> &history) {
    eventsTable_->setRowCount(0);
    eventsTable_->setRowCount(history.size());

    for (int row = 0; row < history.size(); ++row) {
        const auto &item = history.at(row);
        eventsTable_->setItem(row, 0, new QTableWidgetItem(formatTimestamp(item)));
        eventsTable_->setItem(row, 1, new QTableWidgetItem(item.deviceName));
        eventsTable_->setItem(row, 2, new QTableWidgetItem(item.deviceType));
        eventsTable_->setItem(row, 3, new QTableWidgetItem(item.isOn ? tr("ВКЛ") : tr("ВЫКЛ")));
        eventsTable_->setItem(row, 4, new QTableWidgetItem(formatMetricsSummary(item)));
    }
}

QString AnalyticsWidget::inferDeviceType(const QString &deviceName,
                                         const QVector<smarthome::net::DeviceTelemetry> &history) {
    for (const auto &item : history) {
        if (!item.deviceType.isEmpty() && item.deviceType != QStringLiteral("Unknown")) {
            return item.deviceType;
        }
    }
    if (deviceName.startsWith(QStringLiteral("Therm"))) {
        return QStringLiteral("Thermostat");
    }
    if (deviceName.startsWith(QStringLiteral("Hygro"))) {
        return QStringLiteral("Hygrometer");
    }
    if (deviceName.startsWith(QStringLiteral("Plug"))) {
        return QStringLiteral("Plug");
    }
    if (deviceName.startsWith(QStringLiteral("Rad"))) {
        return QStringLiteral("Radiator");
    }
    if (deviceName.startsWith(QStringLiteral("Lamp"))) {
        return QStringLiteral("Lamp");
    }
    return QStringLiteral("Unknown");
}

QString AnalyticsWidget::formatMetricsSummary(const smarthome::net::DeviceTelemetry &item) {
    if (item.deviceType == QStringLiteral("Thermostat")) {
        return QStringLiteral("%1 °C -> %2 °C (%3)")
            .arg(item.currentTemperature, 0, 'f', 1)
            .arg(item.targetTemperature, 0, 'f', 1)
            .arg(item.mode);
    }
    if (item.deviceType == QStringLiteral("Hygrometer")) {
        return QStringLiteral("Влажность %1 % | %2 °C")
            .arg(item.currentHumidity, 0, 'f', 1)
            .arg(item.currentTemperature, 0, 'f', 1);
    }
    if (item.deviceType == QStringLiteral("Plug")) {
        return QStringLiteral("Мощность %1 Вт").arg(item.currentPower, 0, 'f', 1);
    }
    if (item.deviceType == QStringLiteral("Radiator")) {
        return QStringLiteral("Клапан %1 % | %2 °C")
            .arg(item.currentValveLevel)
            .arg(item.currentTemperature, 0, 'f', 1);
    }
    if (item.deviceType == QStringLiteral("Lamp")) {
        return QStringLiteral("Яркость %1 %").arg(item.brightness);
    }
    return QStringLiteral("-");
}

QString AnalyticsWidget::formatTimestamp(const smarthome::net::DeviceTelemetry &item) {
    if (!item.timestampIso.trimmed().isEmpty()) {
        const QDateTime dt = QDateTime::fromString(item.timestampIso.trimmed(), Qt::ISODateWithMs);
        if (dt.isValid()) {
            return dt.toLocalTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
        }
        return item.timestampIso.trimmed();
    }
    if (item.timestamp > 0) {
        return QDateTime::fromMSecsSinceEpoch(item.timestamp)
            .toLocalTime()
            .toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    }
    return QStringLiteral("-");
}

} // namespace smarthome::ui
