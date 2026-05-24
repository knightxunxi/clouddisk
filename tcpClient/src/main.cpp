#include "tcpclient.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>
#include "opewidget.h"
#include "online.h"
#include "friend.h"
#include "sharefile.h"

static void applyLightTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f5f7fa"));
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f0f2f5"));
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::Button, QColor("#ffffff"));
    palette.setColor(QPalette::ButtonText, Qt::black);
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::black);
    palette.setColor(QPalette::PlaceholderText, QColor("#606266"));
    palette.setColor(QPalette::Highlight, QColor("#d8ebff"));
    palette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(palette);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    applyLightTheme(a);

    tcpClient::getInstance().show();


//    ShareFile w;
//    w.show();

    return a.exec();
}
