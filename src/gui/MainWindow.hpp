#pragma once

#include <QMainWindow>

#include <functional>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QComboBox;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    using Completion = std::function<void(bool, const QString&)>;

    QString wvmExecutable() const;
    QString projectDirectory() const;
    bool requireProject();
    void chooseProject();
    void initializeProject();
    void configureProject(const Completion& completion = {});
    void chooseInstallMedia();
    void startVm();
    void sendControlCommand(const QString& command, const QString& label);
    void refreshStatus();
    void loadProjectSettings();
    void runDoctor(const Completion& completion = {});
    void runCommand(
        const QStringList& arguments,
        const QString& label,
        const Completion& completion = {}
    );
    void appendLog(const QString& text);

    QLineEdit* projectPath_ = nullptr;
    QLabel* stateValue_ = nullptr;
    QLabel* accelerationValue_ = nullptr;
    QSpinBox* cores_ = nullptr;
    QComboBox* memory_ = nullptr;
    QComboBox* diskSize_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
};
