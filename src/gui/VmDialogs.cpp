#include "VmDialogs.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <QWizardPage>

namespace {
QWidget *pathField(QLineEdit *field, QWidget *parent, const QString &filter = {},
                   bool directory = false) {
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(field, 1);
    auto *browse = new QPushButton("Browse...", row);
    layout->addWidget(browse);
    QObject::connect(browse, &QPushButton::clicked, row, [field, row, filter, directory] {
        const QString path = directory
                                 ? QFileDialog::getExistingDirectory(row, "Choose location")
                                 : QFileDialog::getOpenFileName(row, "Choose image", {}, filter);
        if (!path.isEmpty())
            field->setText(path);
    });
    return row;
}
} // namespace

NewVmWizard::NewVmWizard(QWidget *parent) : QWizard(parent) {
    setWindowTitle("New Virtual Machine");
    setWizardStyle(QWizard::ClassicStyle);
    resize(620, 440);
    auto *identity = new QWizardPage(this);
    identity->setTitle("Name and operating system");
    identity->setSubTitle("Choose a name and a new folder for this virtual machine.");
    auto *form = new QFormLayout(identity);
    name_ = new QLineEdit("New Virtual Machine", identity);
    directory_ = new QLineEdit(identity);
    const QString defaultRoot =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/WVM";
    directory_->setText(defaultRoot + "/New Virtual Machine");
    family_ = new QComboBox(identity);
    family_->addItems({"Linux", "Windows", "BSD", "Other"});
    form->addRow("Name:", name_);
    form->addRow("Guest operating system:", family_);
    form->addRow("VM folder:", directory_);
    auto *note = new QLabel("Each VM has its own configuration and virtual disk. Existing folders "
                            "are never overwritten.",
                            identity);
    note->setWordWrap(true);
    form->addRow(note);
    connect(name_, &QLineEdit::textChanged, this, [this, defaultRoot](const QString &name) {
        QString folder = name;
        folder.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
        directory_->setText(defaultRoot + "/" + folder);
    });
    addPage(identity);

    auto *hardware = new QWizardPage(this);
    hardware->setTitle("Virtual hardware");
    hardware->setSubTitle("Hardware acceleration is managed automatically by WVM.");
    auto *resources = new QFormLayout(hardware);
    cores_ = new QSpinBox(hardware);
    cores_->setRange(1, qMax(1, QThread::idealThreadCount()));
    cores_->setValue(qMin(4, cores_->maximum()));
    memory_ = new QSpinBox(hardware);
    memory_->setRange(256, 262144);
    memory_->setSingleStep(512);
    memory_->setValue(4096);
    memory_->setSuffix(" MiB");
    disk_ = new QSpinBox(hardware);
    disk_->setRange(1, 16384);
    disk_->setValue(64);
    disk_->setSuffix(" GiB");
    resources->addRow("Processors:", cores_);
    resources->addRow("Memory:", memory_);
    resources->addRow("Virtual disk capacity:", disk_);
    auto *defaults =
        new QLabel("Linux/BSD: VirtIO storage, network and graphics.\nWindows/Other: compatible "
                   "storage and graphics.\nQCOW2 disks allocate space as needed.",
                   hardware);
    defaults->setWordWrap(true);
    resources->addRow(defaults);
    addPage(hardware);

    auto *installation = new QWizardPage(this);
    installation->setTitle("Installation media");
    installation->setSubTitle(
        "Select an installer ISO, import an existing disk, or install later.");
    auto *media = new QFormLayout(installation);
    iso_ = new QLineEdit(installation);
    source_ = new QLineEdit(installation);
    media->addRow("Installer ISO:",
                  pathField(iso_, installation, "ISO images (*.iso);;All files (*)"));
    media->addRow("Existing virtual disk:",
                  pathField(source_, installation,
                            "Virtual disks (*.qcow2 *.raw *.img *.vmdk *.vdi);;All files (*)"));
    auto *importNote =
        new QLabel("Imported disks are converted into a separate QCOW2 copy. The source disk is "
                   "preserved. Snapshots of the source are not copied.",
                   installation);
    importNote->setWordWrap(true);
    media->addRow(importNote);
    addPage(installation);
    setButtonText(QWizard::FinishButton, "Create Virtual Machine");
}

VmEntry NewVmWizard::result() const {
    VmEntry entry;
    entry.directory = QDir(directory_->text().trimmed()).absolutePath();
    entry.family = family_->currentText();
    entry.config.name = name_->text().trimmed().toStdString();
    entry.config.cores = cores_->value();
    entry.config.memory = std::to_string(memory_->value()) + "M";
    entry.config.disk_size = std::to_string(disk_->value()) + "G";
    entry.config.boot_mode = iso_->text().trimmed().isEmpty() ? "disk" : "iso";
    entry.config.boot_path = iso_->text().trimmed().toStdString();
    if (entry.family == "Windows" || entry.family == "Other") {
        entry.config.disk_interface = "ide";
        entry.config.graphics = "std";
        entry.config.network_model = "e1000";
    }
    entry.config.arch = wvm::VMConfig{}.arch;
    return entry;
}
QString NewVmWizard::sourceDisk() const { return source_->text().trimmed(); }
bool NewVmWizard::validateCurrentPage() {
    QString error;
    if (currentId() == 0) {
        const auto vm = result();
        std::string validation;
        if (!wvm::validate_config(vm.config, validation))
            error = QString::fromStdString(validation);
        else if (directory_->text().trimmed().isEmpty() || QFileInfo::exists(vm.directory))
            error = "Enter a new VM folder that does not already exist.";
    } else if (currentId() == 2) {
        if (!iso_->text().trimmed().isEmpty() && !sourceDisk().isEmpty())
            error = "Choose either an installer ISO or an existing disk.";
        else
            for (const auto &path : {iso_->text().trimmed(), sourceDisk()})
                if (!path.isEmpty() && !QFileInfo(path).isFile())
                    error = "The selected image does not exist.";
    }
    if (error.isEmpty())
        return true;
    QMessageBox::warning(this, "New Virtual Machine", error);
    return false;
}

VmSettingsDialog::VmSettingsDialog(const VmEntry &entry, QWidget *parent)
    : QDialog(parent), original_(entry.config) {
    setWindowTitle("Virtual Machine Settings - " + QString::fromStdString(entry.config.name));
    resize(600, 460);
    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    auto *hardware = new QWidget(tabs);
    auto *form = new QFormLayout(hardware);
    name_ = new QLineEdit(QString::fromStdString(entry.config.name), hardware);
    family_ = new QComboBox(hardware);
    family_->addItems({"Linux", "Windows", "BSD", "Other"});
    family_->setCurrentText(entry.family);
    cores_ = new QSpinBox(hardware);
    cores_->setRange(1, 1024);
    cores_->setValue(entry.config.cores);
    memory_ = new QComboBox(hardware);
    memory_->setEditable(true);
    memory_->addItems({"2G", "4G", "8G", "16G", "32G"});
    memory_->setCurrentText(QString::fromStdString(entry.config.memory));
    network_ = new QComboBox(hardware);
    network_->addItem("NAT (user networking)", "user");
    network_->addItem("Disconnected", "none");
    network_->setCurrentIndex(
        network_->findData(QString::fromStdString(entry.config.network_mode)));
    networkModel_ = new QComboBox(hardware);
    networkModel_->addItem("VirtIO", "virtio-net-pci");
    networkModel_->addItem("Intel E1000 (compatible)", "e1000");
    networkModel_->setCurrentIndex(
        networkModel_->findData(QString::fromStdString(entry.config.network_model)));
    diskInterface_ = new QComboBox(hardware);
    diskInterface_->addItem("VirtIO", "virtio");
    diskInterface_->addItem("Compatible (IDE/SATA)", "ide");
    diskInterface_->setCurrentIndex(
        diskInterface_->findData(QString::fromStdString(entry.config.disk_interface)));
    graphics_ = new QComboBox(hardware);
    graphics_->addItem("VirtIO (automatic 3D acceleration)", "virtio");
    graphics_->addItem("Compatible VGA", "std");
    graphics_->setCurrentIndex(graphics_->findData(QString::fromStdString(entry.config.graphics)));
    form->addRow("Name:", name_);
    form->addRow("Guest system:", family_);
    form->addRow("Processors:", cores_);
    form->addRow("Memory:", memory_);
    form->addRow("Network adapter:", network_);
    form->addRow("Storage controller:", diskInterface_);
    form->addRow("Network hardware:", networkModel_);
    form->addRow("Graphics adapter:", graphics_);
    form->addRow("Disk:", new QLabel(VmManager::diskPath(entry), hardware));
    tabs->addTab(hardware, "Hardware");
    auto *mediaPage = new QWidget(tabs);
    auto *mediaForm = new QFormLayout(mediaPage);
    media_ = new QLineEdit(entry.config.boot_mode == "iso"
                               ? QString::fromStdString(entry.config.boot_path)
                               : QString{},
                           mediaPage);
    mediaForm->addRow("Installer ISO:",
                      pathField(media_, mediaPage, "ISO images (*.iso);;All files (*)"));
    auto *bootNote = new QLabel("An ISO boots once for installation. Installer restarts return to "
                                "the system disk. Leave empty to boot from disk.",
                                mediaPage);
    bootNote->setWordWrap(true);
    mediaForm->addRow(bootNote);
    tabs->addTab(mediaPage, "CD/DVD");
    notes_ = new QPlainTextEdit(entry.notes, tabs);
    tabs->addTab(notes_, "Notes");
    root->addWidget(tabs);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        std::string error;
        const auto config = configuration();
        if (!wvm::validate_config(config, error)) {
            QMessageBox::warning(this, "Settings", QString::fromStdString(error));
            return;
        }
        if (config.boot_mode == "iso" &&
            !QFileInfo(QString::fromStdString(config.boot_path)).isFile()) {
            QMessageBox::warning(this, "Settings", "The installer ISO does not exist.");
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
wvm::VMConfig VmSettingsDialog::configuration() const {
    auto config = original_;
    config.name = name_->text().trimmed().toStdString();
    config.cores = cores_->value();
    config.memory = memory_->currentText().trimmed().toStdString();
    config.network_mode = network_->currentData().toString().toStdString();
    config.network_model = networkModel_->currentData().toString().toStdString();
    config.disk_interface = diskInterface_->currentData().toString().toStdString();
    config.graphics = graphics_->currentData().toString().toStdString();
    config.boot_path = media_->text().trimmed().toStdString();
    config.boot_mode = config.boot_path.empty() ? "disk" : "iso";
    return config;
}
QString VmSettingsDialog::family() const { return family_->currentText(); }
QString VmSettingsDialog::notes() const { return notes_->toPlainText(); }
