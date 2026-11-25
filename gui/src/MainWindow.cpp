#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QChart>
#include <QChartView>
#include <QFile>
#include <QLineSeries>
#include <QNetworkAccessManager>
#include <QVBoxLayout>

#ifdef SMARTHOME_HAS_QT_WEBSOCKETS
#include <QWebSocket>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , networkManager_(new QNetworkAccessManager(this))
    , chartView_(nullptr)
#ifdef SMARTHOME_HAS_QT_WEBSOCKETS
    , webSocket_(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
#endif
{
    ui->setupUi(this);
    loadStylesheet();
    setupTelemetryPreviewChart();

#ifdef SMARTHOME_HAS_QT_WEBSOCKETS
    ui->statusValueLabel->setText(tr("Qt 6 initialized (Widgets, Network, WebSockets, Charts)"));
#else
    ui->statusValueLabel->setText(tr("Qt 6 initialized (Widgets, Network, Charts; WebSockets not installed)"));
#endif
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
