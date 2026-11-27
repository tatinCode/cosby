#include "ui/theme.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

void apply_app_theme(QApplication& app){
    if(qEnvironmentVariableIntValue("COSBY_SYSTEM_THEME") != 0){
        return;
    }

    app.setStyle(QStyleFactory::create("Fusion"));

    const QColor window(0x25, 0x25, 0x26);
    const QColor base(0x1e, 0x1e, 0x1e);
    const QColor alternate(0x2a, 0x2a, 0x2d);
    const QColor button(0x33, 0x33, 0x37);
    const QColor text(0xd4, 0xd4, 0xd4);
    const QColor disabled_text(0x6d, 0x6d, 0x6d);
    const QColor accent(0x0e, 0x63, 0x9c);

    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::ToolTipBase, window);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::PlaceholderText, disabled_text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, QColor(0xf4, 0x87, 0x71));
    palette.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Light, QColor(0x45, 0x45, 0x4a));
    palette.setColor(QPalette::Midlight, QColor(0x3a, 0x3a, 0x3e));
    palette.setColor(QPalette::Mid, QColor(0x2d, 0x2d, 0x30));
    palette.setColor(QPalette::Dark, QColor(0x19, 0x19, 0x1a));
    palette.setColor(QPalette::Shadow, Qt::black);

    for(QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}){
        palette.setColor(QPalette::Disabled, role, disabled_text);
    }
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x3a, 0x3a, 0x3e));

    app.setPalette(palette);

    app.setStyleSheet(QStringLiteral(R"QSS(
        QDockWidget::title {
            padding: 6px 8px;
            background: #2d2d30;
        }
        QToolBar {
            spacing: 4px;
            padding: 3px;
            border: none;
        }
        QToolBar QToolButton {
            padding: 4px 8px;
        }
        QListWidget {
            border: none;
        }
    )QSS"));
}
