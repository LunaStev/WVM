#include "MainWindow.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QTimer>

int main(int argc, char **argv) {
    QApplication application(argc, argv);
    application.setApplicationName("WVM");
    application.setApplicationDisplayName("Wave Virtual Machine Manager");
    application.setOrganizationName("LunaStev");
    application.setApplicationVersion("2.0.0");
    application.setWindowIcon(QIcon::fromTheme("wvm"));

    QCommandLineParser parser;
    parser.setApplicationDescription("WVM Workstation: native QEMU/KVM desktop manager");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("project", "Existing VM directory or wvm.xml to open",
                                 "[project]");
    parser.process(application);

    MainWindow window;
    if (!parser.positionalArguments().isEmpty())
        window.openProject(parser.positionalArguments().first());
    window.show();

    const QString screenshotPath = qEnvironmentVariable("WVM_SCREENSHOT_PATH");
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(250, &application, [&application, &window, screenshotPath] {
            window.grab().save(screenshotPath);
            application.quit();
        });
    }
    return application.exec();
}
