#include "MainWindow.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QXmlStreamReader>

#include <memory>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("WVM");
    setMinimumSize(760, 560);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    auto* title = new QLabel("Wave Virtual Machine Manager", central);
    title->setObjectName("title");
    auto* subtitle = new QLabel(
        "Fast local virtual machines powered by QEMU, KVM and VirtIO.",
        central
    );
    subtitle->setObjectName("subtitle");
    root->addWidget(title);
    root->addWidget(subtitle);

    auto* projectBox = new QGroupBox("VM project", central);
    auto* projectLayout = new QHBoxLayout(projectBox);
    projectPath_ = new QLineEdit(projectBox);
    projectPath_->setPlaceholderText("Choose a directory containing wvm.xml");
    auto* browse = new QPushButton("Open", projectBox);
    auto* create = new QPushButton("Create", projectBox);
    projectLayout->addWidget(projectPath_, 1);
    projectLayout->addWidget(browse);
    projectLayout->addWidget(create);
    root->addWidget(projectBox);

    auto* overview = new QFrame(central);
    overview->setObjectName("overview");
    auto* overviewLayout = new QGridLayout(overview);
    overviewLayout->addWidget(new QLabel("State", overview), 0, 0);
    stateValue_ = new QLabel("No project", overview);
    stateValue_->setObjectName("state");
    overviewLayout->addWidget(stateValue_, 0, 1);
    overviewLayout->addWidget(new QLabel("Acceleration", overview), 1, 0);
    accelerationValue_ = new QLabel("Not checked", overview);
    overviewLayout->addWidget(accelerationValue_, 1, 1);
    root->addWidget(overview);

    auto* resources = new QGroupBox("Resources", central);
    auto* resourcesLayout = new QHBoxLayout(resources);
    cores_ = new QSpinBox(resources);
    cores_->setRange(1, 64);
    cores_->setValue(4);
    memory_ = new QComboBox(resources);
    memory_->addItems({"2G", "4G", "8G", "16G", "32G"});
    memory_->setCurrentText("4G");
    diskSize_ = new QComboBox(resources);
    diskSize_->addItems({"32G", "64G", "128G", "256G"});
    diskSize_->setCurrentText("64G");
    auto* apply = new QPushButton("Apply", resources);
    resourcesLayout->addWidget(new QLabel("CPU cores", resources));
    resourcesLayout->addWidget(cores_);
    resourcesLayout->addWidget(new QLabel("Memory", resources));
    resourcesLayout->addWidget(memory_);
    resourcesLayout->addWidget(new QLabel("Disk", resources));
    resourcesLayout->addWidget(diskSize_);
    resourcesLayout->addStretch(1);
    resourcesLayout->addWidget(apply);
    root->addWidget(resources);

    auto* actions = new QHBoxLayout();
    auto* install = new QPushButton("Install OS", central);
    auto* start = new QPushButton("Start", central);
    start->setObjectName("primary");
    auto* reboot = new QPushButton("Restart", central);
    auto* stop = new QPushButton("Shut down", central);
    auto* forceStop = new QPushButton("Force stop", central);
    auto* refresh = new QPushButton("Refresh", central);
    actions->addWidget(install);
    actions->addStretch(1);
    actions->addWidget(start);
    actions->addWidget(reboot);
    actions->addWidget(stop);
    actions->addWidget(forceStop);
    actions->addWidget(refresh);
    root->addLayout(actions);

    log_ = new QPlainTextEdit(central);
    log_->setReadOnly(true);
    log_->setPlaceholderText("WVM activity appears here.");
    log_->setMaximumBlockCount(500);
    root->addWidget(log_, 1);

    setCentralWidget(central);
    setStyleSheet(R"(
        QMainWindow { background: #f5f6f8; }
        QLabel#title { font-size: 24px; font-weight: 700; color: #172033; }
        QLabel#subtitle { color: #687386; margin-bottom: 8px; }
        QGroupBox, QFrame#overview {
            background: white; border: 1px solid #dfe3ea; border-radius: 10px;
            margin-top: 8px; padding: 12px;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }
        QPushButton { padding: 8px 14px; border-radius: 7px; border: 1px solid #cbd2dc; background: white; }
        QPushButton:hover { background: #eef2f8; }
        QPushButton#primary { background: #2f6feb; color: white; border-color: #2f6feb; font-weight: 600; }
        QLineEdit, QComboBox, QSpinBox { padding: 7px; border: 1px solid #cbd2dc; border-radius: 6px; background: white; }
        QPlainTextEdit { border: 1px solid #dfe3ea; border-radius: 8px; background: #111827; color: #d7e0ee; padding: 8px; }
        QLabel#state { font-weight: 700; color: #2f6feb; }
    )");

    connect(browse, &QPushButton::clicked, this, [this] { chooseProject(); });
    connect(create, &QPushButton::clicked, this, [this] { initializeProject(); });
    connect(apply, &QPushButton::clicked, this, [this] { configureProject(); });
    connect(install, &QPushButton::clicked, this, [this] { chooseInstallMedia(); });
    connect(start, &QPushButton::clicked, this, [this] { startVm(); });
    connect(reboot, &QPushButton::clicked, this, [this] {
        sendControlCommand("reboot", "Restart");
    });
    connect(stop, &QPushButton::clicked, this, [this] {
        sendControlCommand("stop", "Shutdown");
    });
    connect(forceStop, &QPushButton::clicked, this, [this] {
        if (requireProject()) {
            runCommand({"stop", projectDirectory(), "--force"}, "Force stop");
        }
    });
    connect(refresh, &QPushButton::clicked, this, [this] { refreshStatus(); });
}

QString MainWindow::wvmExecutable() const {
    const QString bundled = QCoreApplication::applicationDirPath() + "/wvm";
    return QFileInfo(bundled).isExecutable() ? bundled : QStringLiteral("wvm");
}

QString MainWindow::projectDirectory() const {
    return projectPath_->text().trimmed();
}

bool MainWindow::requireProject() {
    if (!projectDirectory().isEmpty()
        && QFileInfo::exists(projectDirectory() + "/wvm.xml")) {
        return true;
    }
    QMessageBox::warning(this, "WVM", "Open or create a VM project first.");
    return false;
}

void MainWindow::chooseProject() {
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        "Open VM project",
        projectDirectory().isEmpty() ? QStandardPaths::writableLocation(
            QStandardPaths::HomeLocation
        ) : projectDirectory()
    );
    if (directory.isEmpty()) {
        return;
    }
    projectPath_->setText(directory);
    loadProjectSettings();
    refreshStatus();
}

void MainWindow::initializeProject() {
    const QString directory = QFileDialog::getExistingDirectory(
        this,
        "Create VM in directory",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
    );
    if (directory.isEmpty()) {
        return;
    }
    projectPath_->setText(directory);
    runCommand({"init"}, "Create VM", [this](const bool ok, const QString&) {
        if (ok) {
            configureProject([this](const bool configured, const QString&) {
                if (configured) {
                    refreshStatus();
                }
            });
        }
    });
}

void MainWindow::configureProject(const Completion& completion) {
    if (!requireProject()) {
        return;
    }
    runCommand(
        {
            "set", "--dist", projectDirectory(),
            "--size", diskSize_->currentText(),
            "--memory", memory_->currentText(),
            "--cores", QString::number(cores_->value())
        },
        "Apply resources",
        completion
    );
}

void MainWindow::chooseInstallMedia() {
    if (!requireProject()) {
        return;
    }
    const QString iso = QFileDialog::getOpenFileName(
        this,
        "Choose installation image",
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        "Installation images (*.iso);;All files (*)"
    );
    if (iso.isEmpty()) {
        return;
    }
    runCommand(
        {"install", projectDirectory(), "--iso", iso},
        "Configure installation",
        [this](const bool ok, const QString&) {
            if (ok) {
                stateValue_->setText("Ready to install");
            }
        }
    );
}

void MainWindow::startVm() {
    if (!requireProject()) {
        return;
    }
    runDoctor([this](const bool healthy, const QString&) {
        if (!healthy) {
            QMessageBox::critical(
                this,
                "Hardware acceleration required",
                "KVM is not available. Review the diagnostic output before starting the VM."
            );
            return;
        }

        qint64 pid = 0;
        const bool started = QProcess::startDetached(
            wvmExecutable(),
            {"run", projectDirectory()},
            projectDirectory(),
            &pid
        );
        appendLog(started
            ? QString("Started VM process %1.\n").arg(pid)
            : QString("Failed to start WVM.\n"));
        if (started) {
            stateValue_->setText("Starting");
            QTimer::singleShot(1200, this, [this] { refreshStatus(); });
        }
    });
}

void MainWindow::sendControlCommand(const QString& command, const QString& label) {
    if (!requireProject()) {
        return;
    }
    runCommand({command, projectDirectory()}, label, [this](const bool, const QString&) {
        QTimer::singleShot(700, this, [this] { refreshStatus(); });
    });
}

void MainWindow::refreshStatus() {
    if (!requireProject()) {
        return;
    }
    loadProjectSettings();
    runCommand({"status", projectDirectory()}, "Status", [this](const bool ok, const QString& output) {
        stateValue_->setText(ok ? output.trimmed() : "Unavailable");
    });
    runDoctor();
}

void MainWindow::loadProjectSettings() {
    QFile file(projectDirectory() + "/wvm.xml");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }

        const QString name = xml.name().toString();
        if (name == "cores") {
            cores_->setValue(xml.readElementText().toInt());
        } else if (name == "memory") {
            const QString value = xml.readElementText();
            if (memory_->findText(value) < 0) {
                memory_->addItem(value);
            }
            memory_->setCurrentText(value);
        } else if (name == "size") {
            const QString value = xml.readElementText();
            if (diskSize_->findText(value) < 0) {
                diskSize_->addItem(value);
            }
            diskSize_->setCurrentText(value);
        }
    }
}

void MainWindow::runDoctor(const Completion& completion) {
    if (!requireProject()) {
        return;
    }
    runCommand({"doctor", projectDirectory()}, "Acceleration check", [this, completion](
        const bool ok,
        const QString& output
    ) {
        accelerationValue_->setText(ok ? "KVM + VirtIO" : "KVM unavailable");
        if (completion) {
            completion(ok, output);
        }
    });
}

void MainWindow::runCommand(
    const QStringList& arguments,
    const QString& label,
    const Completion& completion
) {
    auto* process = new QProcess(this);
    auto output = std::make_shared<QString>();
    auto completed = std::make_shared<bool>(false);
    process->setProgram(wvmExecutable());
    process->setArguments(arguments);
    if (!projectDirectory().isEmpty()) {
        process->setWorkingDirectory(projectDirectory());
    }
    process->setProcessChannelMode(QProcess::MergedChannels);

    appendLog(QString("[%1] %2 %3\n").arg(
        label,
        process->program(),
        arguments.join(' ')
    ));

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, output] {
        const QString chunk = QString::fromLocal8Bit(process->readAllStandardOutput());
        *output += chunk;
        appendLog(chunk);
    });
    connect(
        process,
        &QProcess::errorOccurred,
        this,
        [this, process, output, completed, completion](const QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart || *completed) {
                return;
            }
            *completed = true;
            const QString message = "Failed to start WVM: " + process->errorString() + "\n";
            *output += message;
            appendLog(message);
            if (completion) {
                completion(false, *output);
            }
            process->deleteLater();
        }
    );
    connect(
        process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [process, output, completed, completion](const int code, const QProcess::ExitStatus status) {
            if (*completed) {
                return;
            }
            *completed = true;
            const bool ok = status == QProcess::NormalExit && code == 0;
            if (completion) {
                completion(ok, *output);
            }
            process->deleteLater();
        }
    );
    process->start();
}

void MainWindow::appendLog(const QString& text) {
    if (!text.isEmpty()) {
        log_->moveCursor(QTextCursor::End);
        log_->insertPlainText(text);
        log_->moveCursor(QTextCursor::End);
    }
}
