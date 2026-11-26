#ifndef SMARTHOME_GUI_MAINWINDOW_H
#define SMARTHOME_GUI_MAINWINDOW_H

#include "net/DtoModels.h"

#include <QMainWindow>
#include <QUrl>

class QChartView;
class QFrame;
class QLabel;
class QPushButton;
class QTimer;

namespace smarthome::net {
class ApiClient;
class WsClient;
} // namespace smarthome::net

namespace smarthome::ui {
class DashboardWidget;
} // namespace smarthome::ui

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief Main desktop window for the Smart Home Qt 6 client.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    MainWindow(const QUrl &restBaseUrl,
               const QUrl &wsUrl,
               int healthPollIntervalMs = 5000,
               QWidget *parent = nullptr);
    ~MainWindow() override;

    smarthome::ui::DashboardWidget *dashboardWidget() const { return dashboardWidget_; }
    QString gatewayStatusText() const;
    QString mqttStatusText() const;
    QString wsStatusText() const;
    bool isAlertBannerVisible() const;
    QString alertBannerText() const;

private slots:
    void onHealthReceived(const smarthome::net::HealthStatus &health);
    void onCommandSent(const smarthome::net::CommandResult &result);
    void onApiRequestFailed(const QString &endpoint, int httpStatus, const QString &errorMessage);
    void onWsConnected();
    void onWsDisconnected();
    void onWsReconnecting(int attempt, int delayMs);

private:
    void setupAlertBanner();
    void setupDashboardTab();
    void setupStatusBarIndicators();
    void setupTelemetryPreviewChart();
    void setupConnections();
    void startGatewaySync();
    void showAlertBanner(const QString &message);
    void hideAlertBanner();
    void loadStylesheet();

    static void updateBadgeState(QLabel *label, const QString &text, const QString &state);

    Ui::MainWindow *ui;
    smarthome::net::ApiClient *apiClient_;
    smarthome::net::WsClient *wsClient_;
    smarthome::ui::DashboardWidget *dashboardWidget_;
    QChartView *chartView_;
    QTimer *healthPollTimer_;

    QFrame *alertBanner_;
    QLabel *alertLabel_;
    QPushButton *alertCloseButton_;

    QLabel *gatewayStatusLabel_;
    QLabel *mqttStatusLabel_;
    QLabel *wsStatusLabel_;

    int healthPollIntervalMs_ = 5000;
};

#endif // SMARTHOME_GUI_MAINWINDOW_H
