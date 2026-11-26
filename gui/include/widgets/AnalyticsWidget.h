#ifndef SMARTHOME_GUI_WIDGETS_ANALYTICSWIDGET_H
#define SMARTHOME_GUI_WIDGETS_ANALYTICSWIDGET_H

#include "net/DtoModels.h"

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QChart;
class QChartView;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

namespace smarthome::ui {

/**
 * @brief Analytics & telemetry history tab with device chart and recent events table.
 */
class AnalyticsWidget : public QWidget {
    Q_OBJECT

public:
    explicit AnalyticsWidget(QWidget *parent = nullptr);

    QString selectedDeviceName() const;
    qint64 startTimestampMs() const;
    qint64 endTimestampMs() const;

    QComboBox *deviceComboBox() const { return deviceCombo_; }
    QComboBox *presetComboBox() const { return presetCombo_; }
    QDateTimeEdit *startDateTimeEdit() const { return startDateTimeEdit_; }
    QDateTimeEdit *endDateTimeEdit() const { return endDateTimeEdit_; }
    QPushButton *refreshButton() const { return refreshButton_; }
    QChart *chart() const { return chart_; }
    QTableWidget *eventsTable() const { return eventsTable_; }

public slots:
    void setAvailableDevices(const QVector<smarthome::net::DeviceStatus> &devices);
    void setDeviceHistory(const QString &deviceName,
                          const QVector<smarthome::net::DeviceTelemetry> &history);
    void setAllHistory(const QVector<smarthome::net::DeviceTelemetry> &history);
    void appendLiveTelemetry(const smarthome::net::DeviceTelemetry &telemetry);
    void requestRefresh();

signals:
    void deviceHistoryRequested(const QString &deviceName,
                                qint64 startTsMs,
                                qint64 endTsMs,
                                int limit);
    void allHistoryRequested(int limit);

private:
    void setupFilterBar(QVBoxLayout *rootLayout);
    void setupChartAndTable(QVBoxLayout *rootLayout);
    void applyPresetRange(int presetMinutes);
    void renderDeviceChart();
    void renderEventsTable();
    void updateChartAxes(qint64 minTsMs, qint64 maxTsMs, double minVal, double maxVal, const QString &yTitle);

    static QString inferDeviceType(const QString &deviceName,
                                   const QVector<smarthome::net::DeviceTelemetry> &history);
    static QString formatMetricsSummary(const smarthome::net::DeviceTelemetry &item);
    static QString formatTimestamp(const smarthome::net::DeviceTelemetry &item);

    QComboBox *deviceCombo_;
    QComboBox *presetCombo_;
    QDateTimeEdit *startDateTimeEdit_;
    QDateTimeEdit *endDateTimeEdit_;
    QPushButton *refreshButton_;
    QLabel *chartStatusLabel_;
    QChart *chart_;
    QChartView *chartView_;
    QTableWidget *eventsTable_;

    QString currentDbDeviceName_;
    QVector<smarthome::net::DeviceTelemetry> dbDeviceHistory_;
    QVector<smarthome::net::DeviceTelemetry> dbAllHistory_;
    QHash<QString, QVector<smarthome::net::DeviceTelemetry>> liveHistoryByDevice_;
    QVector<smarthome::net::DeviceTelemetry> liveEventsFeed_;

    bool updatingRange_ = false;
};

} // namespace smarthome::ui

#endif // SMARTHOME_GUI_WIDGETS_ANALYTICSWIDGET_H
