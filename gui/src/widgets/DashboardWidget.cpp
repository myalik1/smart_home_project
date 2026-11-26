#include "widgets/DashboardWidget.h"
#include "widgets/DeviceCardWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace smarthome::ui {

namespace {
constexpr int kGridColumns = 3;

int canonicalRank(const QString &name) {
    if (name == QStringLiteral("Lamp1")) {
        return 0;
    }
    if (name == QStringLiteral("Plug1")) {
        return 1;
    }
    if (name == QStringLiteral("Rad1")) {
        return 2;
    }
    if (name == QStringLiteral("Therm1")) {
        return 3;
    }
    if (name == QStringLiteral("Hygro1")) {
        return 4;
    }
    return 100;
}
} // namespace

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
    , summaryLabel_(new QLabel(tr("Устройства: 0 | Онлайн: 0"), this))
    , refreshButton_(new QPushButton(tr("Обновить"), this))
    , gridHost_(new QWidget(this))
    , cardsGrid_(new QGridLayout(gridHost_)) {
    summaryLabel_->setObjectName(QStringLiteral("dashboardSummary"));
    refreshButton_->setObjectName(QStringLiteral("refreshDevicesButton"));
    gridHost_->setObjectName(QStringLiteral("dashboardGridHost"));

    connect(refreshButton_, &QPushButton::clicked, this, &DashboardWidget::refreshRequested);

    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->addWidget(summaryLabel_);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(refreshButton_);

    cardsGrid_->setContentsMargins(0, 0, 0, 0);
    cardsGrid_->setSpacing(12);
    cardsGrid_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("dashboardScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(gridHost_);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);
    rootLayout->addLayout(toolbarLayout);
    rootLayout->addWidget(scrollArea, 1);
}

int DashboardWidget::onlineCount() const {
    int count = 0;
    for (auto it = cardsByName_.cbegin(); it != cardsByName_.cend(); ++it) {
        if (it.value()->isOnline()) {
            ++count;
        }
    }
    return count;
}

DeviceCardWidget *DashboardWidget::cardForDevice(const QString &deviceName) const {
    return cardsByName_.value(deviceName, nullptr);
}

QString DashboardWidget::summaryText() const {
    return summaryLabel_->text();
}

void DashboardWidget::setDevices(const QVector<smarthome::net::DeviceStatus> &devices) {
    bool layoutChanged = false;
    for (const auto &status : devices) {
        if (status.deviceName.isEmpty()) {
            continue;
        }
        const bool existed = cardsByName_.contains(status.deviceName);
        DeviceCardWidget *card = getOrCreateCard(status.deviceName, status.deviceType);
        card->updateFromStatus(status);
        if (!existed) {
            layoutChanged = true;
        }
    }

    if (layoutChanged) {
        relayoutCards();
    }
    updateSummary();
}

void DashboardWidget::updateTelemetry(const smarthome::net::DeviceTelemetry &telemetry) {
    if (telemetry.deviceName.isEmpty()) {
        return;
    }

    const bool existed = cardsByName_.contains(telemetry.deviceName);
    DeviceCardWidget *card = getOrCreateCard(telemetry.deviceName, telemetry.deviceType);
    card->applyLiveTelemetry(telemetry);

    if (!existed) {
        relayoutCards();
    }
    updateSummary();
}

DeviceCardWidget *DashboardWidget::getOrCreateCard(const QString &deviceName, const QString &deviceType) {
    if (DeviceCardWidget *existing = cardsByName_.value(deviceName, nullptr)) {
        return existing;
    }
    auto *card = new DeviceCardWidget(deviceName, deviceType, gridHost_);
    connect(card, &DeviceCardWidget::commandRequested,
            this, &DashboardWidget::commandRequested);
    cardsByName_.insert(deviceName, card);
    return card;
}

void DashboardWidget::relayoutCards() {
    while (QLayoutItem *item = cardsGrid_->takeAt(0)) {
        delete item;
    }

    const QStringList names = orderedDeviceNames(cardsByName_);
    int index = 0;
    for (const QString &name : names) {
        DeviceCardWidget *card = cardsByName_.value(name);
        const int row = index / kGridColumns;
        const int col = index % kGridColumns;
        cardsGrid_->addWidget(card, row, col);
        ++index;
    }

    for (int col = 0; col < kGridColumns; ++col) {
        cardsGrid_->setColumnStretch(col, 1);
    }
}

void DashboardWidget::updateSummary() {
    summaryLabel_->setText(
        tr("Устройства: %1 | Онлайн: %2").arg(cardsByName_.size()).arg(onlineCount()));
}

QStringList DashboardWidget::orderedDeviceNames(const QHash<QString, DeviceCardWidget *> &cards) {
    QStringList names = cards.keys();
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        const int rankA = canonicalRank(a);
        const int rankB = canonicalRank(b);
        if (rankA != rankB) {
            return rankA < rankB;
        }
        return a < b;
    });
    return names;
}

} // namespace smarthome::ui
