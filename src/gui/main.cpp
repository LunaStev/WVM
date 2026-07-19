#include "MainWindow.hpp"

#include <QApplication>
#include <QIcon>
#include <QTimer>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setApplicationName("WVM");
    application.setApplicationDisplayName("Wave Virtual Machine Manager");
    application.setOrganizationName("LunaStev");
    application.setWindowIcon(QIcon::fromTheme("wvm"));

    MainWindow window;
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
