#include "MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

namespace {
void applyLightThemePalette(QApplication &app) {
    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusion);
    }

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0xf3, 0xf5, 0xf9));
    palette.setColor(QPalette::WindowText, QColor(0x11, 0x18, 0x27));
    palette.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::AlternateBase, QColor(0xf9, 0xfa, 0xfb));
    palette.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipText, QColor(0x11, 0x18, 0x27));
    palette.setColor(QPalette::Text, QColor(0x11, 0x18, 0x27));
    palette.setColor(QPalette::Button, QColor(0xf3, 0xf4, 0xf6));
    palette.setColor(QPalette::ButtonText, QColor(0x11, 0x18, 0x27));
    palette.setColor(QPalette::BrightText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::Highlight, QColor(0x25, 0x63, 0xeb));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    app.setPalette(palette);
}
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SmartHomeGui"));
    app.setOrganizationName(QStringLiteral("SmartHome"));
    applyLightThemePalette(app);

    MainWindow window;
    window.show();

    return app.exec();
}
