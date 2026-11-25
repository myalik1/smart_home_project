#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "net/ApiClient.h"
#include "net/WsClient.h"

#include <QChart>
#include <QChartView>
#include <QFile>
#include <QLineSeries>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , apiClient_(new smarthome::net::ApiClient(QUrl(QStringLiteral("http://127.0.0.1:8001/api/v1")), this))
    , wsClient_(new smarthome::net::WsClient(QUrl(QStringLiteral("ws://127.0.0.1:8001/ws/telemetry")), this))
    , chartView_(nullptr) {
    ui->setupUi(this);
    loadStylesheet();
    setupTelemetryPreviewChart();

    ui->statusValueLabel->setText(tr("Qt 6 initialized (Widgets, Network, WebSockets, Charts)"));
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setupTelemetryPreviewChart() {
    auto *series = new QLineSeries(this);
    series->setName(tr("Telemetry Preview"));
    series->append(0, 21.5);
    series->append(1, 22.0);
    series->append(2, 22.4);

    auto *chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->setTitle(tr("Smart Home Telemetry"));

    chartView_ = new QChartView(chart, ui->chartContainer);
    chartView_->setRenderHint(QPainter::Antialiasing);

    auto *layout = new QVBoxLayout(ui->chartContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(chartView_);
}

void MainWindow::loadStylesheet() {
    QFile styleFile(QStringLiteral(":/styles/app.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }
}
