#include "MainWindow.hpp"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QXmlStreamReader>

#include <memory>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("WVM - Virtual Machine Manager");
    resize(940, 640);
    setMinimumSize(720, 500);

    auto* fileMenu = menuBar()->addMenu("&File");
    auto* newAction = fileMenu->addAction(
        style()->standardIcon(QStyle::SP_FileIcon),
        "&New Virtual Machine..."
    );
    newAction->setShortcut(QKeySequence::New);
    auto* openAction = fileMenu->addAction(
        style()->standardIcon(QStyle::SP_DirOpenIcon),
        "&Open Virtual Machine..."
    );
    openAction->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);

    auto* vmMenu = menuBar()->addMenu("&Virtual Machine");
    auto* installAction = vmMenu->addAction("Select Installation Media...");
    vmMenu->addSeparator();
    auto* startAction = vmMenu->addAction(
        style()->standardIcon(QStyle::SP_MediaPlay),
        "Power &On"
    );
    auto* restartAction = vmMenu->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload),
        "&Restart Guest"
    );
    auto* shutdownAction = vmMenu->addAction("Shut &Down Guest");
    auto* forceStopAction = vmMenu->addAction(
        style()->standardIcon(QStyle::SP_MediaStop),
        "Power O&ff"
    );
    vmMenu->addSeparator();
    auto* refreshAction = vmMenu->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload),
        "&Refresh"
    );
    refreshAction->setShortcut(QKeySequence::Refresh);

    auto* helpMenu = menuBar()->addMenu("&Help");
    auto* aboutAction = helpMenu->addAction("&About WVM");

    auto* toolbar = addToolBar("Virtual Machine");
    toolbar->setObjectName("mainToolBar");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addSeparator();
    toolbar->addAction(startAction);
    toolbar->addAction(forceStopAction);
    toolbar->addSeparator();
    toolbar->addAction(refreshAction);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    auto* libraryPane = new QWidget(splitter);
    auto* libraryLayout = new QVBoxLayout(libraryPane);
    libraryLayout->setContentsMargins(6, 6, 6, 6);
    libraryLayout->setSpacing(4);
    auto* libraryTitle = new QLabel("Library", libraryPane);
    QFont libraryFont = libraryTitle->font();
    libraryFont.setBold(true);
    libraryTitle->setFont(libraryFont);
    libraryLayout->addWidget(libraryTitle);
    library_ = new QTreeWidget(libraryPane);
    library_->setHeaderHidden(true);
    library_->setRootIsDecorated(true);
    auto* hostItem = new QTreeWidgetItem(library_);
    hostItem->setText(0, "My Computer");
    hostItem->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));
    currentVmItem_ = new QTreeWidgetItem(hostItem);
    currentVmItem_->setText(0, "No virtual machine selected");
    currentVmItem_->setDisabled(true);
    hostItem->setExpanded(true);
    library_->setCurrentItem(hostItem);
    libraryLayout->addWidget(library_, 1);

    auto* workspace = new QWidget(splitter);
    auto* workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(8, 6, 8, 8);
    workspaceLayout->setSpacing(6);

    auto* heading = new QHBoxLayout();
    auto* machineIcon = new QLabel(workspace);
    machineIcon->setPixmap(
        style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(32, 32)
    );
    auto* machineTitle = new QLabel("Virtual Machine", workspace);
    QFont titleFont = machineTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    machineTitle->setFont(titleFont);
    heading->addWidget(machineIcon);
    heading->addWidget(machineTitle);
    heading->addStretch(1);
    workspaceLayout->addLayout(heading);

    auto* tabs = new QTabWidget(workspace);
    auto* summary = new QWidget(tabs);
    auto* summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(10, 10, 10, 10);
    summaryLayout->setSpacing(8);

    auto* detailsBox = new QGroupBox("Virtual Machine Details", summary);
    auto* details = new QFormLayout(detailsBox);
    projectPath_ = new QLineEdit(detailsBox);
    projectPath_->setReadOnly(true);
    projectPath_->setPlaceholderText("No project is open");
    auto* locationRow = new QWidget(detailsBox);
    auto* locationLayout = new QHBoxLayout(locationRow);
    locationLayout->setContentsMargins(0, 0, 0, 0);
    locationLayout->setSpacing(4);
    auto* browse = new QPushButton("Browse...", locationRow);
    locationLayout->addWidget(projectPath_, 1);
    locationLayout->addWidget(browse);
    stateValue_ = new QLabel("Not opened", detailsBox);
    accelerationValue_ = new QLabel("Not checked", detailsBox);
    details->addRow("Location:", locationRow);
    details->addRow("State:", stateValue_);
    details->addRow("Virtualization:", accelerationValue_);
    summaryLayout->addWidget(detailsBox);

    auto* hardwareBox = new QGroupBox("Virtual Hardware", summary);
    auto* hardware = new QGridLayout(hardwareBox);
    hardware->addWidget(new QLabel("Device", hardwareBox), 0, 0);
    hardware->addWidget(new QLabel("Configuration", hardwareBox), 0, 1);
    hardware->setColumnStretch(1, 1);
    hardware->addWidget(new QLabel("Processors", hardwareBox), 1, 0);
    cores_ = new QSpinBox(hardwareBox);
    cores_->setRange(1, 64);
    cores_->setValue(4);
    cores_->setSuffix(" cores");
    hardware->addWidget(cores_, 1, 1);
    hardware->addWidget(new QLabel("Memory", hardwareBox), 2, 0);
    memory_ = new QComboBox(hardwareBox);
    memory_->addItems({"2G", "4G", "8G", "16G", "32G"});
    memory_->setCurrentText("4G");
    hardware->addWidget(memory_, 2, 1);
    hardware->addWidget(new QLabel("Hard Disk (VirtIO)", hardwareBox), 3, 0);
    diskSize_ = new QComboBox(hardwareBox);
    diskSize_->addItems({"32G", "64G", "128G", "256G"});
    diskSize_->setCurrentText("64G");
    hardware->addWidget(diskSize_, 3, 1);
    auto* apply = new QPushButton("Apply Hardware Settings", hardwareBox);
    hardware->addWidget(apply, 4, 1, Qt::AlignRight);
    summaryLayout->addWidget(hardwareBox);

    auto* commandsBox = new QGroupBox("Commands", summary);
    auto* commands = new QHBoxLayout(commandsBox);
    auto* install = new QPushButton("Install OS...", commandsBox);
    auto* start = new QPushButton(
        style()->standardIcon(QStyle::SP_MediaPlay),
        "Power On",
        commandsBox
    );
    auto* reboot = new QPushButton("Restart Guest", commandsBox);
    auto* stop = new QPushButton("Shut Down", commandsBox);
    auto* forceStop = new QPushButton("Power Off", commandsBox);
    commands->addWidget(install);
    commands->addStretch(1);
    commands->addWidget(start);
    commands->addWidget(reboot);
    commands->addWidget(stop);
    commands->addWidget(forceStop);
    summaryLayout->addWidget(commandsBox);
    summaryLayout->addStretch(1);
    tabs->addTab(summary, "Summary");
    workspaceLayout->addWidget(tabs, 1);

    splitter->addWidget(libraryPane);
    splitter->addWidget(workspace);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({210, 730});
    setCentralWidget(splitter);

    auto* taskDock = new QDockWidget("Tasks and Messages", this);
    taskDock->setObjectName("taskDock");
    taskDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    log_ = new QPlainTextEdit(taskDock);
    log_->setReadOnly(true);
    log_->setPlaceholderText("Command output and VM events are displayed here.");
    log_->setMaximumBlockCount(500);
    taskDock->setWidget(log_);
    addDockWidget(Qt::BottomDockWidgetArea, taskDock);
    resizeDocks({taskDock}, {150}, Qt::Vertical);
    statusBar()->showMessage("Ready");

    connect(browse, &QPushButton::clicked, this, [this] { chooseProject(); });
    connect(newAction, &QAction::triggered, this, [this] { initializeProject(); });
    connect(openAction, &QAction::triggered, this, [this] { chooseProject(); });
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    connect(apply, &QPushButton::clicked, this, [this] { configureProject(); });
    connect(install, &QPushButton::clicked, this, [this] { chooseInstallMedia(); });
    connect(installAction, &QAction::triggered, this, [this] { chooseInstallMedia(); });
    connect(start, &QPushButton::clicked, this, [this] { startVm(); });
    connect(startAction, &QAction::triggered, this, [this] { startVm(); });
    connect(reboot, &QPushButton::clicked, this, [this] {
        sendControlCommand("reboot", "Restart");
    });
    connect(restartAction, &QAction::triggered, this, [this] {
        sendControlCommand("reboot", "Restart");
    });
    connect(stop, &QPushButton::clicked, this, [this] {
        sendControlCommand("stop", "Shutdown");
    });
    connect(shutdownAction, &QAction::triggered, this, [this] {
        sendControlCommand("stop", "Shutdown");
    });
    connect(forceStop, &QPushButton::clicked, this, [this] {
        if (requireProject()) {
            runCommand({"stop", projectDirectory(), "--force"}, "Force stop");
        }
    });
    connect(forceStopAction, &QAction::triggered, this, [this] {
        if (requireProject()) {
            runCommand({"stop", projectDirectory(), "--force"}, "Force stop");
        }
    });
    connect(refreshAction, &QAction::triggered, this, [this] { refreshStatus(); });
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this,
            "About WVM",
            "WVM Virtual Machine Manager\nQEMU/KVM desktop client"
        );
    });
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
    updateProjectPresentation();
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
    updateProjectPresentation();
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
    stateValue_->setText("Starting");
    runCommand(
        {"run", projectDirectory()},
        "Power on",
        [this](const bool ok, const QString& output) {
            stateValue_->setText("Stopped");
            if (!ok && output.contains("firmware", Qt::CaseInsensitive)) {
                QMessageBox::critical(
                    this,
                    "UEFI virtualization is disabled",
                    "WVM can configure Linux automatically, but hardware "
                    "virtualization is switched off in the computer firmware.\n\n"
                    "Restart the computer, open UEFI/BIOS Setup, enable SVM Mode "
                    "(AMD) or Intel Virtualization Technology, save, and boot "
                    "Linux again."
                );
            } else if (!ok) {
                QMessageBox::critical(
                    this,
                    "Virtual machine did not start",
                    "See Tasks and Messages for the exact error."
                );
            }
            QTimer::singleShot(300, this, [this] { refreshStatus(); });
        }
    );
    QTimer::singleShot(1200, this, [this] { refreshStatus(); });
}

void MainWindow::updateProjectPresentation() {
    const QFileInfo directory(projectDirectory());
    if (projectDirectory().isEmpty()) {
        currentVmItem_->setText(0, "No virtual machine selected");
        currentVmItem_->setDisabled(true);
        return;
    }

    const QString name = directory.fileName().isEmpty()
        ? projectDirectory()
        : directory.fileName();
    currentVmItem_->setText(0, name);
    currentVmItem_->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));
    currentVmItem_->setDisabled(false);
    library_->setCurrentItem(currentVmItem_);
    setWindowTitle(name + " - WVM Virtual Machine Manager");
    statusBar()->showMessage("Opened " + projectDirectory(), 5000);
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
    statusBar()->showMessage(label + "...");

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
        [this, process, output, completed, completion, label](
            const QProcess::ProcessError error
        ) {
            if (error != QProcess::FailedToStart || *completed) {
                return;
            }
            *completed = true;
            const QString message = "Failed to start WVM: " + process->errorString() + "\n";
            *output += message;
            appendLog(message);
            statusBar()->showMessage("Failed to start " + label, 5000);
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
        [this, process, output, completed, completion, label](
            const int code,
            const QProcess::ExitStatus status
        ) {
            if (*completed) {
                return;
            }
            *completed = true;
            const bool ok = status == QProcess::NormalExit && code == 0;
            statusBar()->showMessage(ok ? label + " completed" : label + " failed", 5000);
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
