#ifndef SMARTHOME_GUI_MAINWINDOW_H
#define SMARTHOME_GUI_MAINWINDOW_H

#include <QMainWindow>

class QNetworkAccessManager;
class QChartView;

#ifdef SMARTHOME_HAS_QT_WEBSOCKETS
class QWebSocket;
#endif

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
    QNetworkAccessManager *networkManager_;
    QChartView *chartView_;
#ifdef SMARTHOME_HAS_QT_WEBSOCKETS
    QWebSocket *webSocket_;
#endif
};

#endif // SMARTHOME_GUI_MAINWINDOW_H
