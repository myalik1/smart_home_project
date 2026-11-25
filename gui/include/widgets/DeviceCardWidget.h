#ifndef SMARTHOME_GUI_WIDGETS_DEVICECARDWIDGET_H
#define SMARTHOME_GUI_WIDGETS_DEVICECARDWIDGET_H

#include "net/DtoModels.h"

#include <QFrame>
#include <QPair>
#include <QString>
#include <QVector>

class QGridLayout;
class QLabel;

namespace smarthome::ui {

/**
 * @brief Dashboard card displaying a single smart home device status and profile metrics.
 */
class DeviceCardWidget : public QFrame {
    Q_OBJECT

public:
    explicit DeviceCardWidget(const QString &deviceName,
                              const QString &deviceType,
                              QWidget *parent = nullptr);

    QString deviceName() const { return deviceName_; }
    QString deviceType() const { return deviceType_; }
    bool isOnline() const { return isOnline_; }

    QString onlineBadgeText() const;
    QString lastUpdatedText() const;
    QString metricsText() const;

    void updateFromStatus(const smarthome::net::DeviceStatus &status);
    void applyLiveTelemetry(const smarthome::net::DeviceTelemetry &telemetry);

private:
    void setOnlineState(bool online);
    void updateTimestampLabel(const smarthome::net::DeviceTelemetry &telemetry);
    void rebuildMetrics(const smarthome::net::DeviceTelemetry &telemetry);
    void clearMetricsLayout();

    static QVector<QPair<QString, QString>> extractProfileMetrics(
        const QString &deviceType,
        const smarthome::net::DeviceTelemetry &telemetry);
    static QString formatTimestamp(const smarthome::net::DeviceTelemetry &telemetry);
    static QString formatDeviceTypeTitle(const QString &deviceType);

    QString deviceName_;
    QString deviceType_;
    bool isOnline_ = false;

    QLabel *nameLabel_;
    QLabel *typeLabel_;
    QLabel *onlineBadgeLabel_;
    QLabel *updatedAtLabel_;
    QGridLayout *metricsLayout_;
    QVector<QPair<QString, QString>> currentMetrics_;
};

} // namespace smarthome::ui

#endif // SMARTHOME_GUI_WIDGETS_DEVICECARDWIDGET_H
