#include "MainWindow.hpp"
#include "ConsoleView.hpp"
#include "HostCapabilities.hpp"
#include "VmDialogs.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
void initializeTable(QTableWidget *table, const QStringList &headings) {
    table->setColumnCount(headings.size());
    table->setHorizontalHeaderLabels(headings);
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setAlternatingRowColors(true);
}
void addRow(QTableWidget *table, const QStringList &values, const QString &id = {}) {
    const int row = table->rowCount();
    table->insertRow(row);
    for (int i = 0; i < values.size(); ++i) {
        auto *item = new QTableWidgetItem(values[i]);
        item->setData(Qt::UserRole, id);
        table->setItem(row, i, item);
    }
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), manager_({}, this) {
    setWindowTitle("WVM Workstation 2");
    resize(1100, 760);
    setMinimumSize(860, 580);
    auto *file = menuBar()->addMenu("&File");
    auto *create =
        file->addAction(style()->standardIcon(QStyle::SP_FileIcon), "&New Virtual Machine...");
    create->setShortcut(QKeySequence::New);
    auto *open =
        file->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon), "&Open Virtual Machine...");
    open->setShortcut(QKeySequence::Open);
    file->addSeparator();
    auto *openFolder = file->addAction("Open VM Folder");
    file->addSeparator();
    auto *exit = file->addAction("E&xit");
    exit->setShortcut(QKeySequence::Quit);
    auto *vm = menuBar()->addMenu("&Virtual Machine");
    start_ = vm->addAction(style()->standardIcon(QStyle::SP_MediaPlay), "Power &On");
    shutdown_ = vm->addAction("Shut Down Guest");
    stop_ = vm->addAction(style()->standardIcon(QStyle::SP_MediaStop), "Power O&ff...");
    pause_ = vm->addAction(style()->standardIcon(QStyle::SP_MediaPause), "&Pause");
    reset_ = vm->addAction("&Restart Guest...");
    vm->addSeparator();
    auto *cad = vm->addAction("Send Ctrl+Alt+Delete");
    vm->addSeparator();
    settings_ =
        vm->addAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), "&Settings...");
    clone_ = vm->addAction("&Clone Virtual Machine...");
    auto *expandDisk = vm->addAction("Expand Virtual Disk...");
    remove_ = vm->addAction("Remove from Library...");
    auto *snapshotMenu = menuBar()->addMenu("&Snapshots");
    snapshotCreate_ = snapshotMenu->addAction("Take Disk Snapshot...");
    snapshotRestore_ = snapshotMenu->addAction("Restore Selected Snapshot...");
    snapshotDelete_ = snapshotMenu->addAction("Delete Selected Snapshot...");
    auto *refreshSnapshots = snapshotMenu->addAction("Refresh Snapshots");
    auto *view = menuBar()->addMenu("&View");
    auto *fullscreen = view->addAction("&Full Screen");
    fullscreen->setCheckable(true);
    fullscreen->setShortcut(Qt::Key_F11);
    auto *refresh =
        view->addAction(style()->standardIcon(QStyle::SP_BrowserReload), "Refresh Library");
    refresh->setShortcut(QKeySequence::Refresh);
    auto *help = menuBar()->addMenu("&Help");
    auto *host = help->addAction("Host Hardware and Diagnostics...");
    auto *about = help->addAction("About WVM");
    auto *toolbar = addToolBar("Workstation");
    toolbar->setObjectName("workstationToolbar");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(create);
    toolbar->addAction(open);
    toolbar->addSeparator();
    toolbar->addAction(start_);
    toolbar->addAction(pause_);
    toolbar->addAction(shutdown_);
    toolbar->addSeparator();
    toolbar->addAction(settings_);
    toolbar->addAction(snapshotCreate_);
    auto *splitter = new QSplitter(this);
    splitter->setChildrenCollapsible(false);
    auto *libraryPane = new QWidget(splitter);
    auto *libraryLayout = new QVBoxLayout(libraryPane);
    libraryLayout->setContentsMargins(6, 6, 6, 6);
    auto *title = new QLabel("Library", libraryPane);
    auto font = title->font();
    font.setBold(true);
    title->setFont(font);
    libraryLayout->addWidget(title);
    search_ = new QLineEdit(libraryPane);
    search_->setPlaceholderText("Find virtual machine...");
    search_->setClearButtonEnabled(true);
    libraryLayout->addWidget(search_);
    library_ = new QTreeWidget(libraryPane);
    library_->setHeaderLabels({"Virtual Machine", "State"});
    library_->setColumnWidth(0, 180);
    library_->setContextMenuPolicy(Qt::CustomContextMenu);
    libraryLayout->addWidget(library_, 1);
    auto *workspace = new QWidget(splitter);
    auto *root = new QVBoxLayout(workspace);
    root->setContentsMargins(8, 6, 8, 6);
    auto *heading = new QHBoxLayout();
    name_ = new QLabel("My Computer", workspace);
    font = name_->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    name_->setFont(font);
    state_ = new QLabel(workspace);
    heading->addWidget(name_);
    heading->addStretch();
    heading->addWidget(state_);
    root->addLayout(heading);
    tabs_ = new QTabWidget(workspace);
    overview_ = new QTableWidget(tabs_);
    initializeTable(overview_, {"Virtual Machine", "Guest", "State", "Processors", "Memory"});
    tabs_->addTab(overview_, "My Computer");
    console_ = new ConsoleView(tabs_);
    tabs_->addTab(console_, "Console");
    auto *summary = new QWidget(tabs_);
    auto *summaryLayout = new QVBoxLayout(summary);
    auto *details = new QFormLayout();
    location_ = new QLabel(summary);
    location_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    location_->setWordWrap(true);
    details->addRow("VM location:", location_);
    summaryLayout->addLayout(details);
    hardware_ = new QTableWidget(summary);
    initializeTable(hardware_, {"Device", "Configuration"});
    summaryLayout->addWidget(hardware_, 1);
    auto *edit = new QPushButton("Edit Virtual Machine Settings...", summary);
    summaryLayout->addWidget(edit, 0, Qt::AlignLeft);
    tabs_->addTab(summary, "Summary");
    auto *snapshotPage = new QWidget(tabs_);
    auto *snapshotLayout = new QVBoxLayout(snapshotPage);
    auto *explanation = new QLabel("Disk snapshots are managed while the VM is powered off. They "
                                   "preserve disk contents, not running memory.",
                                   snapshotPage);
    explanation->setWordWrap(true);
    snapshotLayout->addWidget(explanation);
    snapshots_ = new QTableWidget(snapshotPage);
    initializeTable(snapshots_, {"Name", "Created", "ID"});
    snapshotLayout->addWidget(snapshots_, 1);
    auto *snapshotButtons = new QHBoxLayout();
    for (auto *action : {snapshotCreate_, snapshotRestore_, snapshotDelete_, refreshSnapshots}) {
        auto *button = new QPushButton(action->text(), snapshotPage);
        snapshotButtons->addWidget(button);
        connect(button, &QPushButton::clicked, action, &QAction::trigger);
        connect(action, &QAction::changed, button,
                [action, button] { button->setEnabled(action->isEnabled()); });
    }
    snapshotButtons->addStretch();
    snapshotLayout->addLayout(snapshotButtons);
    tabs_->addTab(snapshotPage, "Snapshots");
    notes_ = new QPlainTextEdit(tabs_);
    notes_->setReadOnly(true);
    tabs_->addTab(notes_, "Notes");
    root->addWidget(tabs_, 1);
    auto *consoleHint = new QLabel("Console: click to use keyboard and mouse; Ctrl+Alt releases "
                                   "input. F11 toggles full screen.",
                                   workspace);
    consoleHint->setWordWrap(true);
    root->addWidget(consoleHint);
    splitter->addWidget(libraryPane);
    splitter->addWidget(workspace);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 820});
    setCentralWidget(splitter);
    auto *tasks = new QDockWidget("Tasks and Messages", this);
    tasks->setObjectName("taskDock");
    log_ = new QPlainTextEdit(tasks);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    tasks->setWidget(log_);
    addDockWidget(Qt::BottomDockWidgetArea, tasks);
    resizeDocks({tasks}, {130}, Qt::Vertical);
    view->addAction(tasks->toggleViewAction());
    connect(create, &QAction::triggered, this, &MainWindow::newMachine);
    connect(open, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Open Virtual Machine", {}, "WVM configuration (wvm.xml);;XML files (*.xml)");
        if (!path.isEmpty())
            openProject(path);
    });
    connect(openFolder, &QAction::triggered, this, [this] {
        if (const auto *entry = manager_.entry(selected_))
            QDesktopServices::openUrl(QUrl::fromLocalFile(entry->directory));
    });
    connect(exit, &QAction::triggered, this, &QWidget::close);
    connect(start_, &QAction::triggered, this, [this] {
        tabs_->setCurrentIndex(1);
        manager_.start(selected_);
    });
    connect(shutdown_, &QAction::triggered, this,
            [this] { manager_.control(selected_, "system_powerdown"); });
    connect(pause_, &QAction::triggered, this, [this] {
        manager_.control(selected_, manager_.state(selected_) == "Paused" ? "cont" : "stop");
    });
    connect(stop_, &QAction::triggered, this, [this] {
        if (QMessageBox::question(this, "Power Off",
                                  "Power off this VM immediately? Unsaved guest work will be lost.",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) == QMessageBox::Yes)
            manager_.control(selected_, "quit");
    });
    connect(reset_, &QAction::triggered, this, [this] {
        if (QMessageBox::question(
                this, "Restart", "Reset this VM immediately? Unsaved guest work will be lost.",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            manager_.control(selected_, "system_reset");
    });
    connect(cad, &QAction::triggered, console_, &ConsoleView::sendCtrlAltDelete);
    connect(settings_, &QAction::triggered, this, &MainWindow::settings);
    connect(edit, &QPushButton::clicked, settings_, &QAction::trigger);
    connect(clone_, &QAction::triggered, this, &MainWindow::cloneMachine);
    connect(expandDisk, &QAction::triggered, this, [this] {
        if (!manager_.entry(selected_))
            return;
        bool accepted = false;
        const int size = QInputDialog::getInt(
            this, "Expand Virtual Disk", "New total capacity (GiB):", 128, 1, 16384, 1, &accepted);
        if (accepted)
            manager_.expandDisk(selected_, size);
    });
    connect(remove_, &QAction::triggered, this, &MainWindow::removeMachine);
    connect(snapshotCreate_, &QAction::triggered, this, [this] { snapshot("-c"); });
    connect(snapshotRestore_, &QAction::triggered, this, [this] { snapshot("-a"); });
    connect(snapshotDelete_, &QAction::triggered, this, [this] { snapshot("-d"); });
    connect(refreshSnapshots, &QAction::triggered, this,
            [this] { manager_.refreshSnapshots(selected_); });
    connect(fullscreen, &QAction::toggled, this, [this](bool enabled) {
        if (enabled)
            showFullScreen();
        else
            showNormal();
    });
    connect(refresh, &QAction::triggered, this, &MainWindow::rebuildLibrary);
    connect(host, &QAction::triggered, this, &MainWindow::hostInformation);
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, "About WVM",
                           "WVM Workstation 2.0\nNative Qt desktop manager for QEMU/KVM\nMPL-2.0");
    });
    connect(library_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (item)
            select(item->data(0, Qt::UserRole).toString());
    });
    connect(library_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        auto *item = library_->itemAt(point);
        if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
            return;
        library_->setCurrentItem(item);
        QMenu menu(this);
        for (auto *action : {start_, shutdown_, pause_, settings_, clone_, remove_})
            menu.addAction(action);
        menu.exec(library_->viewport()->mapToGlobal(point));
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        auto *root = library_->topLevelItem(0);
        if (!root)
            return;
        for (int i = 0; i < root->childCount(); ++i)
            root->child(i)->setHidden(
                !root->child(i)->text(0).contains(search_->text(), Qt::CaseInsensitive));
    });
    connect(overview_, &QTableWidget::cellDoubleClicked, this,
            [this](int row) { select(overview_->item(row, 0)->data(Qt::UserRole).toString()); });
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 3)
            manager_.refreshSnapshots(selected_);
    });
    connect(&manager_, &VmManager::changed, this, [this] {
        rebuildLibrary();
        if (closing_ && !manager_.anyActive())
            QTimer::singleShot(0, this, &QWidget::close);
    });
    connect(&manager_, &VmManager::selected, this, &MainWindow::select);
    connect(&manager_, &VmManager::log, this, &MainWindow::appendLog);
    connect(&manager_, &VmManager::errorOccurred, this, [this](const QString &message) {
        appendLog(message);
        QMessageBox::warning(this, "WVM", message.left(4000));
    });
    connect(&manager_, &VmManager::snapshotsChanged, this,
            [this](const QString &id, const QJsonArray &array) {
                if (id != selected_)
                    return;
                snapshots_->setRowCount(0);
                for (const auto &value : array) {
                    const auto snapshot = value.toObject();
                    addRow(snapshots_, {snapshot.value("name").toString(),
                                        QDateTime::fromSecsSinceEpoch(
                                            snapshot.value("date-sec").toVariant().toLongLong())
                                            .toString(Qt::ISODate),
                                        snapshot.value("id").toString()});
                }
            });
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    selected_ = settings.value("selectedVm").toString();
    if (!manager_.entry(selected_))
        selected_.clear();
    rebuildLibrary();
    if (!selected_.isEmpty())
        select(selected_);
    appendLog(
        "WVM Workstation 2 ready. QEMU and QMP are managed directly by the desktop application.");
}

void MainWindow::rebuildLibrary() {
    const QSignalBlocker blocker(library_);
    library_->clear();
    auto *root = new QTreeWidgetItem(library_, {"My Computer"});
    root->setIcon(0, style()->standardIcon(QStyle::SP_ComputerIcon));
    root->setExpanded(true);
    overview_->setRowCount(0);
    for (const auto &entry : manager_.entries()) {
        const QString name = QString::fromStdString(entry.config.name),
                      state = manager_.state(entry.id);
        auto *item = new QTreeWidgetItem(root, {name, state});
        item->setData(0, Qt::UserRole, entry.id);
        item->setIcon(0, style()->standardIcon(state == "Running" ? QStyle::SP_MediaPlay
                                                                  : QStyle::SP_ComputerIcon));
        item->setToolTip(0, entry.directory);
        item->setHidden(!name.contains(search_->text(), Qt::CaseInsensitive));
        if (entry.id == selected_)
            library_->setCurrentItem(item);
        addRow(overview_,
               {name, entry.family, state, QString::number(entry.config.cores),
                QString::fromStdString(entry.config.memory)},
               entry.id);
    }
    if (selected_.isEmpty())
        library_->setCurrentItem(root);
    updateSelection();
}
void MainWindow::select(const QString &id) {
    if (selected_ != id) {
        selected_ = id;
        connectedConsole_.clear();
        console_->disconnectConsole();
        snapshots_->setRowCount(0);
    }
    if (auto *root = library_->topLevelItem(0))
        for (int i = 0; i < root->childCount(); ++i)
            if (root->child(i)->data(0, Qt::UserRole).toString() == id &&
                library_->currentItem() != root->child(i)) {
                QSignalBlocker blocker(library_);
                library_->setCurrentItem(root->child(i));
            }
    tabs_->setCurrentIndex(id.isEmpty() ? 0 : 2);
    updateSelection();
}
void MainWindow::updateSelection() {
    const auto *entry = manager_.entry(selected_);
    const auto *session = manager_.session(selected_);
    const bool active = session && session->active();
    const bool editable = entry && !active && !manager_.busy(selected_);
    const QString state = entry ? manager_.state(selected_) : QString{};
    start_->setEnabled(editable);
    settings_->setEnabled(editable);
    clone_->setEnabled(editable);
    remove_->setEnabled(editable);
    shutdown_->setEnabled(active);
    stop_->setEnabled(active);
    pause_->setEnabled(active);
    reset_->setEnabled(active);
    pause_->setText(state == "Paused" ? "Resume" : "Pause");
    snapshotCreate_->setEnabled(editable && entry->config.disk_format == "qcow2");
    snapshotRestore_->setEnabled(editable && entry->config.disk_format == "qcow2");
    snapshotDelete_->setEnabled(editable && entry->config.disk_format == "qcow2");
    for (int i = 1; i < tabs_->count(); ++i)
        tabs_->setTabEnabled(i, entry != nullptr);
    hardware_->setRowCount(0);
    if (!entry) {
        name_->setText("My Computer");
        state_->clear();
        location_->clear();
        notes_->clear();
        tabs_->setCurrentIndex(0);
    } else {
        name_->setText(QString::fromStdString(entry->config.name));
        state_->setText(state);
        location_->setText(entry->directory);
        notes_->setPlainText(entry->notes);
        addRow(hardware_, {"Operating system", entry->family});
        addRow(hardware_, {"Processors", QString::number(entry->config.cores)});
        addRow(hardware_, {"Memory", QString::fromStdString(entry->config.memory)});
        addRow(hardware_, {"Hard disk", VmManager::diskPath(*entry)});
        addRow(hardware_,
               {"Storage controller", QString::fromStdString(entry->config.disk_interface)});
        addRow(hardware_,
               {"Network adapter", entry->config.network_mode == "user" ? "NAT" : "Disconnected"});
        addRow(hardware_, {"Graphics", QString::fromStdString(entry->config.graphics)});
        addRow(hardware_, {"CD/DVD", entry->config.boot_mode == "iso"
                                         ? QString::fromStdString(entry->config.boot_path)
                                         : "Disconnected"});
        addRow(hardware_, {"Acceleration", "KVM (automatically managed)"});
        if (active && connectedConsole_ != session->consolePath()) {
            connectedConsole_ = session->consolePath();
            console_->open(connectedConsole_);
        } else if (!active && !connectedConsole_.isEmpty()) {
            connectedConsole_.clear();
            console_->disconnectConsole();
        }
    }
    int running = 0;
    for (const auto &vm : manager_.entries())
        if (manager_.session(vm.id) && manager_.session(vm.id)->active())
            ++running;
    statusBar()->showMessage(QString("%1 virtual machines   |   %2 running   |   %3")
                                 .arg(manager_.entries().size())
                                 .arg(running)
                                 .arg(state));
}
void MainWindow::openProject(const QString &path) {
    const QFileInfo info(path);
    const QString directory = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    QString error;
    const QString id = manager_.importProject(directory, error);
    if (id.isEmpty())
        QMessageBox::warning(this, "Open VM", error);
    else
        select(id);
}
void MainWindow::newMachine() {
    NewVmWizard wizard(this);
    if (wizard.exec() == QDialog::Accepted)
        manager_.create(wizard.result(), wizard.sourceDisk());
}
void MainWindow::settings() {
    const auto *entry = manager_.entry(selected_);
    if (!entry)
        return;
    VmSettingsDialog dialog(*entry, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString error;
        if (!manager_.saveConfiguration(selected_, dialog.configuration(), dialog.family(),
                                        dialog.notes(), error))
            QMessageBox::warning(this, "Settings", error);
    }
}
void MainWindow::cloneMachine() {
    const auto *entry = manager_.entry(selected_);
    if (!entry)
        return;
    const QString name =
        QInputDialog::getText(this, "Clone Virtual Machine", "Name:", QLineEdit::Normal,
                              QString::fromStdString(entry->config.name) + " - Clone");
    if (name.trimmed().isEmpty())
        return;
    const QString parent =
        QFileDialog::getExistingDirectory(this, "Choose parent directory for the clone");
    if (parent.isEmpty())
        return;
    QString folder = name;
    folder.replace('/', '_');
    folder.replace('\\', '_');
    manager_.clone(selected_, QDir(parent).filePath(folder), name);
}
void MainWindow::removeMachine() {
    if (QMessageBox::question(
            this, "Remove from Library",
            "Remove this VM from the library? The VM directory and all disks are kept.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    QString error;
    if (!manager_.removeFromLibrary(selected_, error))
        QMessageBox::warning(this, "Library", error);
    else
        select({});
}
void MainWindow::snapshot(const QString &operation) {
    QString name;
    if (operation == "-c")
        name = QInputDialog::getText(this, "Take Disk Snapshot", "Snapshot name:");
    else if (snapshots_->currentRow() >= 0)
        name = snapshots_->item(snapshots_->currentRow(), 0)->text();
    if (name.trimmed().isEmpty())
        return;
    if (operation != "-c" &&
        QMessageBox::question(
            this, "Snapshot",
            operation == "-a"
                ? "Restore this disk snapshot? Changes made after the snapshot will be lost."
                : "Delete this disk snapshot?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    manager_.snapshot(selected_, operation, name);
}
void MainWindow::appendLog(const QString &message) {
    if (!message.trimmed().isEmpty())
        log_->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " +
                              message.trimmed());
}
void MainWindow::hostInformation() {
    const auto status = wvm::inspect_kvm("x86_64");
    QMessageBox::information(
        this, "Host Hardware",
        QString(
            "Host: %1\nCPU: %2\nVirtualization exposed: %3\nKVM device: %4\nGPU render node: "
            "%5\n\nWVM prepares KVM modules and permissions automatically when a VM is powered on.")
            .arg(QString::fromStdString(status.host_architecture),
                 QString::fromStdString(status.cpu_vendor),
                 status.cpu_virtualization ? "Yes" : "No",
                 status.device_accessible ? "Ready" : "Not accessible",
                 wvm::gpu_acceleration_available() ? "Available" : "Not available"));
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (manager_.anyActive()) {
        event->ignore();
        if (closing_)
            return;
        if (manager_.hasOperations()) {
            QMessageBox::information(
                this, "Work in Progress",
                "Wait for the current disk operation to finish before closing WVM.");
            return;
        }
        if (QMessageBox::question(
                this, "Exit WVM",
                "Power off all running VMs and exit? Unsaved guest work will be lost.",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            closing_ = true;
            for (const auto &entry : manager_.entries())
                manager_.control(entry.id, "quit");
        }
        return;
    }
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("selectedVm", selected_);
    QMainWindow::closeEvent(event);
}
