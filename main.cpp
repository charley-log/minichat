#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(240, 240, 240));
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::Button, QColor(230, 230, 230));
    palette.setColor(QPalette::ButtonText, Qt::black);
    a.setPalette(palette);

    // 加载全局 QSS 样式表（从 .qrc 资源文件中读取）
    QFile qss(":/style/stylesheet.qss");
    if (qss.open(QFile::ReadOnly)) {
        a.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qss.close();
    } else {
        qDebug() << "qss 加载失败，请检查路径";
    }

    MainWindow w;
    w.show();
    return a.exec();
}
