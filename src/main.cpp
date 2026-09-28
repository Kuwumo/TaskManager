#include "ui/MainWindow.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("TaskManager"));
    QApplication::setApplicationDisplayName(QStringLiteral("任务管理器"));
    qRegisterMetaType<SystemSnapshot>();

    QTranslator qtTranslator;
    const QString translationDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    bool loaded = qtTranslator.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"), translationDir);
    if (!loaded) {
        loaded = qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                                   QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    }
    if (loaded) {
        application.installTranslator(&qtTranslator);
    }

    MainWindow window;
    window.show();
    return application.exec();
}
