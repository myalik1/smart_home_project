#ifndef SMARTHOME_GUI_WIDGETS_DEVICECARDWIDGET_H
#define SMARTHOME_GUI_WIDGETS_DEVICECARDWIDGET_H

#include "net/DtoModels.h"

#include <QFrame>
#include <QPair>
#include <QString>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;
class QTimer;
class QVBoxLayout;

namespace smarthome::ui {

/**
 * @brief Dashboard card displaying device status, telemetry metrics, and interactive controls.
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

    QPushButton *onButton() const { return onButton_; }
    QPushButton *offButton() const { return offButton_; }
    QSlider *levelSlider() const { return levelSlider_; }
    QDoubleSpinBox *targetTempSpinBox() const { return targetTempSpinBox_; }
    QComboBox *modeComboBox() const { return modeComboBox_; }

    void setDebounceIntervalMs(int intervalMs);

    void updateFromStatus(const smarthome::net::DeviceStatus &status);
    void applyLiveTelemetry(const smarthome::net::DeviceTelemetry &telemetry);

signals:
    void commandRequested(const QString &deviceName, const smarthome::net::CommandRequest &command);

private:
    void setOnlineState(bool online);
    void updateTimestampLabel(const smarthome::net::DeviceTelemetry &telemetry);
    void rebuildMetrics(const smarthome::net::DeviceTelemetry &telemetry);
    void rebuildControlsIfNeeded();
    void syncControlsFromTelemetry(const smarthome::net::DeviceTelemetry &telemetry);
    void addPowerButtons();
    void addLevelSliderControl(const QString &labelText, const QString &commandName);
    void addThermostatControls();

    void clearLayout(class QLayout *layout);
    void dispatchLevelSliderCommand();
    void dispatchTargetTempCommand();

    static bool isActuatorType(const QString &deviceType);
    static QVector<QPair<QString, QString>> extractProfileMetrics(
        const QString &deviceType,
        const smarthome::net::DeviceTelemetry &telemetry);
    static QString formatTimestamp(const smarthome::net::DeviceTelemetry &telemetry);
    static QString formatDeviceTypeTitle(const QString &deviceType);

    QString deviceName_;
    QString deviceType_;
    QString controlsBuiltForType_;
    bool isOnline_ = false;
    bool updatingControls_ = false;
    int debounceIntervalMs_ = 250;

    QLabel *nameLabel_;
    QLabel *typeLabel_;
    QLabel *onlineBadgeLabel_;
    QLabel *updatedAtLabel_;
    QGridLayout *metricsLayout_;
    QVBoxLayout *controlsLayout_;

    QPushButton *onButton_ = nullptr;
    QPushButton *offButton_ = nullptr;
    QSlider *levelSlider_ = nullptr;
    QLabel *levelValueLabel_ = nullptr;
    QString levelCommandName_;
    QTimer *sliderDebounceTimer_;
    QDoubleSpinBox *targetTempSpinBox_ = nullptr;
    QTimer *tempDebounceTimer_;
    QComboBox *modeComboBox_ = nullptr;

    QVector<QPair<QString, QString>> currentMetrics_;
};

} // namespace smarthome::ui

#endif // SMARTHOME_GUI_WIDGETS_DEVICECARDWIDGET_H
