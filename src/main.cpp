#include "ui/MainWindow.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

// 程序入口。先装好 Qt 自带翻译，再显示主窗口。
// 采样从 MainWindow 构造函数里的 SamplerThread::start() 开始。
int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("TaskManager"));
    QApplication::setApplicationDisplayName(QStringLiteral("任务管理器"));
    qRegisterMetaType<SystemSnapshot>();

    // QMessageBox 的 Yes/No 来自 Qt 翻译，不是我们自己的字符串。
    // 优先用 windeployqt 放到 exe 旁的 translations，否则退回 Qt 安装目录。
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
