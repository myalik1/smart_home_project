#ifndef SMARTHOME_GUI_MAINWINDOW_H
#define SMARTHOME_GUI_MAINWINDOW_H

#include <QMainWindow>

class QChartView;

namespace smarthome::net {
class ApiClient;
class WsClient;
} // namespace smarthome::net

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
    ~MainWindow() override;

private:
    void setupTelemetryPreviewChart();
    void loadStylesheet();

    Ui::MainWindow *ui;
    smarthome::net::ApiClient *apiClient_;
    smarthome::net::WsClient *wsClient_;
    QChartView *chartView_;
};

#endif // SMARTHOME_GUI_MAINWINDOW_H
