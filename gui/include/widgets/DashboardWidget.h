#ifndef SMARTHOME_GUI_WIDGETS_DASHBOARDWIDGET_H
#define SMARTHOME_GUI_WIDGETS_DASHBOARDWIDGET_H

#include "net/DtoModels.h"

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QLabel;
class QPushButton;

namespace smarthome::ui {

class DeviceCardWidget;

/**
 * @brief Main dashboard tab displaying all device cards and live telemetry updates.
 */
class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);

    int deviceCount() const { return cardsByName_.size(); }
    int onlineCount() const;
    DeviceCardWidget *cardForDevice(const QString &deviceName) const;
    QString summaryText() const;

public slots:
    void setDevices(const QVector<smarthome::net::DeviceStatus> &devices);
    void updateTelemetry(const smarthome::net::DeviceTelemetry &telemetry);

signals:
    void refreshRequested();

private:
    void relayoutCards();
    void updateSummary();
    DeviceCardWidget *getOrCreateCard(const QString &deviceName, const QString &deviceType);

    static QStringList orderedDeviceNames(const QHash<QString, DeviceCardWidget *> &cards);

    QLabel *summaryLabel_;
    QPushButton *refreshButton_;
    QWidget *gridHost_;
    QGridLayout *cardsGrid_;
    QHash<QString, DeviceCardWidget *> cardsByName_;
};

} // namespace smarthome::ui

#endif // SMARTHOME_GUI_WIDGETS_DASHBOARDWIDGET_H
