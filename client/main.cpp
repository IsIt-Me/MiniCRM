#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Имена приложения используются Qt для путей и настроек (QSettings).
    QApplication::setOrganizationName(QStringLiteral("MiniCRM"));
    QApplication::setApplicationName(QStringLiteral("MiniCRM"));

    MainWindow window;
    window.show();

    // Цикл событий обязателен: без него не работают QTimer (автосохранение),
    // сигналы модели и редактирование по двойному клику.
    return app.exec();
}
