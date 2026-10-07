#include "VmManager.hpp"
#include "HostCapabilities.hpp"
#include "QemuCommandBuilder.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <memory>

namespace {
std::filesystem::path nativePath(const QString &path) {
    return std::filesystem::path(path.toStdString());
}
bool writeConfig(const QString &path, const wvm::VMConfig &config, QString &error) {
    std::string validation;
    if (!wvm::validate_config(config, validation)) {
        error = QString::fromStdString(validation);
        return false;
    }
    const QString temporary = path + "." + QUuid::createUuid().toString(QUuid::Id128);
    if (!wvm::save_config(nativePath(temporary), config)) {
        error = "Cannot write VM configuration.";
        return false;
    }
    QFile source(temporary);
    QSaveFile destination(path);
    const bool opened = source.open(QIODevice::ReadOnly) && destination.open(QIODevice::WriteOnly);
    const QByteArray data = opened ? source.readAll() : QByteArray{};
    const bool written = opened && destination.write(data) == data.size() && destination.commit();
    source.close();
    QFile::remove(temporary);
    if (!written)
        error = "Cannot atomically save VM configuration: " + destination.errorString();
    return written;
}
QString helperPath() {
    const QString directory = QCoreApplication::applicationDirPath();
    for (const QString &candidate :
         {directory + "/wvm-host-setup", directory + "/../libexec/wvm/wvm-host-setup"}) {
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }
    return QStringLiteral("/usr/libexec/wvm/wvm-host-setup");
}
} // namespace

QmpConnection::QmpConnection(QObject *parent) : QObject(parent) {
    connect(&socket_, &QLocalSocket::readyRead, this, &QmpConnection::readMessages);
    connect(&socket_, &QLocalSocket::disconnected, this, [this] {
        ready_ = false;
        pending_.clear();
        emit readyChanged(false);
    });
}

void QmpConnection::open(const QString &path) {
    if (socket_.state() != QLocalSocket::UnconnectedState)
        return;
    buffer_.clear();
    socket_.connectToServer(path);
}

void QmpConnection::close() {
    socket_.abort();
    ready_ = false;
    pending_.clear();
    buffer_.clear();
}

void QmpConnection::command(const QString &name, const QJsonObject &arguments) {
    if (socket_.state() != QLocalSocket::ConnectedState)
        return;
    const int id = ++sequence_;
    pending_.insert(id, name);
    QJsonObject request{{"execute", name}, {"id", id}};
    if (!arguments.isEmpty())
        request.insert("arguments", arguments);
    socket_.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
}

void QmpConnection::readMessages() {
    buffer_ += socket_.readAll();
    if (buffer_.size() > 4 * 1024 * 1024) {
        close();
        emit failure("QMP response exceeds the permitted size.");
        return;
    }
    while (buffer_.contains('\n')) {
        const int end = buffer_.indexOf('\n');
        const auto document = QJsonDocument::fromJson(buffer_.left(end));
        buffer_.remove(0, end + 1);
        if (!document.isObject())
            continue;
        const QJsonObject object = document.object();
        if (object.contains("QMP")) {
            command("qmp_capabilities");
        } else if (object.contains("event")) {
            emit eventReceived(object.value("event").toString());
        } else if (object.contains("id")) {
            const QString name = pending_.take(object.value("id").toInt());
            if (object.contains("error")) {
                emit failure(object.value("error").toObject().value("desc").toString());
            } else if (name == "qmp_capabilities") {
                ready_ = true;
                emit readyChanged(true);
            } else {
                emit response(name, object.value("return").toObject());
            }
        }
    }
}

VmSession::VmSession(VmEntry entry, QObject *parent)
    : QObject(parent), entry_(std::move(entry)), qmp_(this) {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString hash =
        QCryptographicHash::hash(entry_.directory.toUtf8(), QCryptographicHash::Sha256)
            .toHex()
            .left(24);
    runtime_ = base + "/wvm/" + hash;
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.setStandardInputFile(QProcess::nullDevice());
    connect(&process_, &QProcess::readyReadStandardOutput, this,
            [this] { emit log(QString::fromLocal8Bit(process_.readAllStandardOutput())); });
    connect(&process_, &QProcess::started, this, [this] {
        launched_ = true;
        poll_.start();
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        poll_.stop();
        if (runtimeLock_)
            runtimeLock_->unlock();
        setState("Error");
        emit log(process_.errorString());
        emit stopped(false);
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus exitStatus) {
                poll_.stop();
                qmp_.close();
                if (launched_) {
                    QFile::remove(runtime_ + "/qmp.sock");
                    QFile::remove(consolePath());
                }
                if (runtimeLock_)
                    runtimeLock_->unlock();
                const bool success = code == 0 && exitStatus == QProcess::NormalExit;
                setState(success ? "Stopped" : "Error");
                emit stopped(success);
            });
    connect(&qmp_, &QmpConnection::readyChanged, this, [this](bool ready) {
        if (ready) {
            for (const auto &command : queuedControls_)
                qmp_.command(command);
            queuedControls_.clear();
            qmp_.command("query-status");
            emit consoleReady(consolePath());
        }
    });
    connect(
        &qmp_, &QmpConnection::response, this,
        [this](const QString &name, const QJsonObject &response) {
            if (name == "query-status") {
                const QString status = response.value("status").toString();
                setState(status == "running" ? "Running" : status == "paused" ? "Paused" : status);
            }
        });
    connect(&qmp_, &QmpConnection::eventReceived, this,
            [this](const QString &) { qmp_.command("query-status"); });
    connect(&qmp_, &QmpConnection::failure, this, &VmSession::log);
    poll_.setInterval(500);
    connect(&poll_, &QTimer::timeout, this, [this] {
        if (qmp_.ready()) {
            poll_.setInterval(2000);
            qmp_.command("query-status");
        } else {
            qmp_.open(runtime_ + "/qmp.sock");
        }
    });
}

bool VmSession::start(QString &error, bool useKvm) {
    if (active()) {
        error = "This virtual machine is already running.";
        return false;
    }
    std::string validation;
    if (!wvm::validate_config(entry_.config, validation)) {
        error = QString::fromStdString(validation);
        return false;
    }
    if (!QFileInfo::exists(VmManager::diskPath(entry_))) {
        error = "The virtual disk is missing.";
        return false;
    }
    if (entry_.config.boot_mode == "iso" || entry_.config.boot_mode == "img") {
        const QString media =
            QDir(entry_.directory)
                .absoluteFilePath(QString::fromStdString(entry_.config.boot_path));
        if (!QFileInfo(media).isFile()) {
            error = "Installation media is missing.";
            return false;
        }
    }
    if (!QDir().mkpath(runtime_)) {
        error = "Cannot create the VM runtime directory.";
        return false;
    }
    QFile::setPermissions(QFileInfo(runtime_).absolutePath(),
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QFile::setPermissions(runtime_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    runtimeLock_ = std::make_unique<QLockFile>(runtime_ + "/vm.lock");
    runtimeLock_->setStaleLockTime(0);
    if (!runtimeLock_->tryLock(0)) {
        error = "This VM is already open in another running WVM process.";
        return false;
    }
    if (QFileInfo::exists(runtime_ + "/qmp.sock") || QFileInfo::exists(consolePath())) {
        for (const auto &path : {runtime_ + "/qmp.sock", consolePath()}) {
            if (!QFileInfo::exists(path))
                continue;
            QLocalSocket probe;
            probe.connectToServer(path);
            if (probe.waitForConnected(150) ||
                probe.error() != QLocalSocket::ConnectionRefusedError ||
                !std::filesystem::is_socket(nativePath(path))) {
                runtimeLock_->unlock();
                error = "An existing QEMU instance owns this VM's runtime socket.";
                return false;
            }
            QFile::remove(path);
        }
    }
    if (consolePath().toUtf8().size() > 100) {
        runtimeLock_->unlock();
        error = "The runtime path is too long.";
        return false;
    }

    wvm::VMConfig config = entry_.config;
    const bool gpu = useKvm && config.graphics == "virtio" && wvm::gpu_acceleration_available();
    config.display = "none";
    auto command = wvm::build_qemu_command(nativePath(entry_.directory), config, useKvm,
                                           nativePath(runtime_ + "/qmp.sock"));
    // The embedded console uses a private local socket, never a TCP listener.
    if (gpu) {
        for (std::size_t i = 0; i + 1 < command.size(); ++i) {
            if (command[i] == "-vga") {
                command[i] = "-device";
                command[i + 1] = "virtio-vga-gl";
            }
            if (command[i] == "-display")
                command[i + 1] = "egl-headless";
        }
    } else {
        for (std::size_t i = 0; i + 1 < command.size(); ++i) {
            if (command[i] == "-vga")
                command[i + 1] = config.graphics;
        }
    }
    QString socketName = consolePath();
    socketName.replace(",", ",,");
    command.insert(command.end(), {"-vnc", "unix:" + socketName.toStdString(), "-device",
                                   "qemu-xhci,id=xhci", "-device", "usb-tablet"});
    QStringList arguments;
    for (std::size_t i = 1; i < command.size(); ++i)
        arguments << QString::fromStdString(command[i]);
    process_.setProgram(QString::fromStdString(command.front()));
    process_.setArguments(arguments);
    process_.setWorkingDirectory(entry_.directory);
    emit log(QString::fromStdString(wvm::format_command(command)));
    setState("Starting");
    process_.start();
    return true;
}

VmSession::~VmSession() = default;

void VmSession::setState(const QString &state) {
    if (state_ == state)
        return;
    state_ = state;
    emit changed();
}

bool VmSession::active() const {
    return state_ == "Starting" || process_.state() != QProcess::NotRunning;
}

void VmSession::control(const QString &command) {
    if (!QStringList{"quit", "stop", "cont", "system_reset", "system_powerdown"}.contains(command))
        return;
    if (!qmp_.ready()) {
        if (active() && queuedControls_.size() < 8)
            queuedControls_ << command;
        return;
    }
    qmp_.command(command);
}

VmManager::VmManager(QString storage, QObject *parent) : QObject(parent) {
    storage_ =
        storage.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/library.json"
            : std::move(storage);
    load();
}

const VmEntry *VmManager::entry(const QString &id) const {
    for (const auto &item : entries_)
        if (item.id == id)
            return &item;
    return nullptr;
}

VmSession *VmManager::session(const QString &id) const { return sessions_.value(id, nullptr); }
bool VmManager::busy(const QString &id) const { return operations_.contains(id); }
bool VmManager::anyActive() const {
    for (auto *session : sessions_)
        if (session->active())
            return true;
    return !operations_.isEmpty();
}
QString VmManager::state(const QString &id) const {
    if (busy(id))
        return "Working";
    const auto *vm = session(id);
    if (vm)
        return vm->state();
    if (entry(id) && !entry(id)->configError.isEmpty())
        return "Invalid configuration";
    return entry(id) && QFileInfo::exists(entry(id)->directory + "/wvm.xml") ? "Stopped"
                                                                             : "Missing";
}
QString VmManager::diskPath(const VmEntry &entry) {
    return QDir(entry.directory).absoluteFilePath(QString::fromStdString(entry.config.disk_path));
}

void VmManager::load() {
    QFile file(storage_);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        storageValid_ = false;
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject() || document.object().value("version").toInt() != 2 ||
        !document.object().value("machines").isArray()) {
        storageValid_ = false;
        return;
    }
    for (const auto &value : document.object().value("machines").toArray()) {
        const QJsonObject object = value.toObject();
        VmEntry item;
        item.id = object.value("id").toString();
        item.directory = object.value("directory").toString();
        item.family = object.value("family").toString("Linux");
        item.notes = object.value("notes").toString();
        if (item.id.isEmpty() || item.directory.isEmpty() || entry(item.id)) {
            storageValid_ = false;
            continue;
        }
        std::string validation;
        if (!wvm::load_config(nativePath(item.directory + "/wvm.xml"), item.config))
            item.configError = "Cannot read wvm.xml.";
        else if (!wvm::validate_config(item.config, validation))
            item.configError = QString::fromStdString(validation);
        entries_.append(item);
    }
}

bool VmManager::persist(QString &error) {
    if (!storageValid_) {
        error = "The library file is unreadable or invalid. It has been preserved at " + storage_;
        return false;
    }
    if (!QDir().mkpath(QFileInfo(storage_).absolutePath())) {
        error = "Cannot create library storage.";
        return false;
    }
    QJsonArray machines;
    for (const auto &item : entries_)
        machines.append(QJsonObject{{"id", item.id},
                                    {"directory", item.directory},
                                    {"family", item.family},
                                    {"notes", item.notes}});
    QSaveFile file(storage_);
    const QByteArray data =
        QJsonDocument(QJsonObject{{"version", 2}, {"machines", machines}}).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        error = "Cannot save the VM library: " + file.errorString();
        return false;
    }
    return true;
}

QString VmManager::importProject(const QString &directory, QString &error) {
    VmEntry item;
    item.directory = QFileInfo(directory).canonicalFilePath();
    for (const auto &existing : entries_)
        if (existing.directory == item.directory)
            return existing.id;
    std::string validation;
    if (!wvm::load_config(nativePath(item.directory + "/wvm.xml"), item.config) ||
        !wvm::validate_config(item.config, validation)) {
        error = "Cannot open VM configuration. " + QString::fromStdString(validation);
        return {};
    }
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entries_.append(item);
    if (!persist(error)) {
        entries_.removeLast();
        return {};
    }
    emit changed();
    emit selected(item.id);
    return item.id;
}

void VmManager::runTool(const QString &id, const QString &program, const QStringList &arguments,
                        Completion completion) {
    if (busy(id)) {
        emit errorOccurred("Another operation is already in progress.");
        return;
    }
    auto *process = new QProcess(this);
    operations_.insert(id, process);
    auto output = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, output] {
        const QByteArray chunk = process->readAllStandardOutput();
        if (output->size() < 4 * 1024 * 1024)
            *output += chunk;
        emit log(QString::fromLocal8Bit(chunk));
    });
    auto complete = [this, id, process, output, finished,
                     completion = std::move(completion)](bool ok) {
        if (*finished)
            return;
        *finished = true;
        *output += process->readAllStandardOutput();
        operations_.remove(id);
        if (!ok)
            emit errorOccurred(process->error() == QProcess::FailedToStart
                                   ? process->errorString()
                                   : QString::fromLocal8Bit(*output));
        completion(ok, *output);
        process->deleteLater();
        emit changed();
    };
    connect(process, &QProcess::errorOccurred, this, [complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            complete(false);
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [complete](int code, QProcess::ExitStatus status) {
                complete(code == 0 && status == QProcess::NormalExit);
            });
    emit log(program + " " + arguments.join(' '));
    process->start(program, arguments);
    emit changed();
}

void VmManager::create(VmEntry item, const QString &sourceDisk) {
    item.directory = QDir(item.directory).absolutePath();
    QString error;
    if (!storageValid_) {
        emit errorOccurred("The library is unreadable. Repair " + storage_ +
                           " before creating VMs.");
        return;
    }
    std::string validation;
    if (!wvm::validate_config(item.config, validation)) {
        emit errorOccurred(QString::fromStdString(validation));
        return;
    }
    if (QFileInfo::exists(item.directory)) {
        emit errorOccurred("Choose a new VM directory; existing directories are not overwritten.");
        return;
    }
    if (!QDir().mkpath(item.directory)) {
        emit errorOccurred("Cannot create VM directory.");
        return;
    }
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString disk = diskPath(item);
    const QStringList arguments =
        sourceDisk.isEmpty()
            ? QStringList{"create", "-f", "qcow2", disk,
                          QString::fromStdString(item.config.disk_size)}
            : QStringList{"convert", "-O", "qcow2", QFileInfo(sourceDisk).absoluteFilePath(), disk};
    runTool(item.id, "qemu-img", arguments, [this, item](bool ok, const QByteArray &) {
        if (!ok)
            return;
        QString error;
        if (!writeConfig(item.directory + "/wvm.xml", item.config, error)) {
            emit errorOccurred(error);
            return;
        }
        entries_.append(item);
        if (!persist(error)) {
            entries_.removeLast();
            emit errorOccurred(error);
            return;
        }
        emit changed();
        emit selected(item.id);
    });
}

void VmManager::clone(const QString &id, const QString &directory, const QString &name) {
    const auto *source = entry(id);
    if (!source || busy(id) || (session(id) && session(id)->active())) {
        emit errorOccurred("Power off the VM before cloning it.");
        return;
    }
    VmEntry copy = *source;
    copy.directory = directory;
    copy.config.name = name.toStdString();
    copy.config.disk_path = "disk.qcow2";
    copy.config.disk_format = "qcow2";
    copy.config.boot_mode = "disk";
    copy.config.boot_path.clear();
    create(copy, diskPath(*source));
}

bool VmManager::saveConfiguration(const QString &id, const wvm::VMConfig &config,
                                  const QString &family, const QString &notes, QString &error) {
    if (busy(id) || (session(id) && session(id)->active())) {
        error = "Power off the VM before changing virtual hardware.";
        return false;
    }
    for (auto &item : entries_) {
        if (item.id != id)
            continue;
        const VmEntry previous = item;
        if (!writeConfig(item.directory + "/wvm.xml", config, error))
            return false;
        item.config = config;
        item.configError.clear();
        item.family = family;
        item.notes = notes;
        if (!persist(error)) {
            item = previous;
            QString rollbackError;
            writeConfig(item.directory + "/wvm.xml", item.config, rollbackError);
            return false;
        }
        if (auto *existing = sessions_.take(id))
            existing->deleteLater();
        emit changed();
        return true;
    }
    error = "VM not found.";
    return false;
}

bool VmManager::removeFromLibrary(const QString &id, QString &error) {
    if (busy(id) || (session(id) && session(id)->active())) {
        error = "Power off the VM before removing it from the library.";
        return false;
    }
    const auto before = entries_;
    entries_.removeIf([&id](const VmEntry &item) { return item.id == id; });
    if (!persist(error)) {
        entries_ = before;
        return false;
    }
    if (auto *existing = sessions_.take(id))
        existing->deleteLater();
    emit changed();
    return true;
}

void VmManager::start(const QString &id) {
    const auto *item = entry(id);
    if (!item || busy(id) || (session(id) && session(id)->active()))
        return;
    if (!item->configError.isEmpty()) {
        emit errorOccurred(item->configError);
        return;
    }
    const auto status = wvm::inspect_kvm(item->config.arch);
    if (!status.native_architecture) {
        emit errorOccurred("This host cannot hardware-accelerate the selected guest architecture.");
        return;
    }
    if (status.available()) {
        launch(id);
        return;
    }
    if (!status.cpu_virtualization && !status.device_exists) {
        emit errorOccurred(
            "Hardware virtualization is not exposed to Linux. Enable AMD SVM / Intel VT-x in UEFI. "
            "If this host is itself a virtual machine, enable nested virtualization on the outer "
            "host.");
        return;
    }
    runTool(id, helperPath(), {"host-setup"}, [this, id](bool ok, const QByteArray &) {
        const auto *item = entry(id);
        if (ok && item && wvm::inspect_kvm(item->config.arch).available())
            launch(id);
    });
}

void VmManager::launch(const QString &id) {
    const auto *item = entry(id);
    if (!item)
        return;
    if (auto *old = sessions_.take(id))
        old->deleteLater();
    auto *vm = new VmSession(*item, this);
    sessions_.insert(id, vm);
    connect(vm, &VmSession::changed, this, &VmManager::changed);
    connect(vm, &VmSession::log, this, [this, id](const QString &message) {
        const auto *item = entry(id);
        emit log((item ? QString::fromStdString(item->config.name) : id) + ": " + message);
    });
    connect(vm, &VmSession::stopped, this, [this, id](bool successful) {
        if (successful) {
            for (auto &item : entries_) {
                if (item.id == id && item.config.boot_mode == "iso") {
                    item.config.boot_mode = "disk";
                    item.config.boot_path.clear();
                    QString error;
                    if (!writeConfig(item.directory + "/wvm.xml", item.config, error))
                        emit errorOccurred(error);
                }
            }
        }
        emit changed();
    });
    QString error;
    if (!vm->start(error))
        emit errorOccurred(error);
    emit changed();
}

void VmManager::control(const QString &id, const QString &command) {
    if (auto *vm = session(id))
        vm->control(command);
}

void VmManager::refreshSnapshots(const QString &id) {
    const auto *item = entry(id);
    if (!item || busy(id) || (session(id) && session(id)->active()))
        return;
    runTool(id, "qemu-img", {"info", "--output=json", diskPath(*item)},
            [this, id](bool ok, const QByteArray &output) {
                if (ok)
                    emit snapshotsChanged(
                        id, QJsonDocument::fromJson(output).object().value("snapshots").toArray());
            });
}

void VmManager::snapshot(const QString &id, const QString &operation, const QString &name) {
    const auto *item = entry(id);
    if (!item || busy(id) || (session(id) && session(id)->active())) {
        emit errorOccurred("Power off the VM before managing disk snapshots.");
        return;
    }
    if (item->config.disk_format != "qcow2" || name.isEmpty() || name.size() > 128 ||
        !QStringList{"-c", "-a", "-d"}.contains(operation)) {
        emit errorOccurred("Snapshots require a QCOW2 disk and a valid name.");
        return;
    }
    runTool(id, "qemu-img", {"snapshot", operation, name, diskPath(*item)},
            [this, id](bool ok, const QByteArray &) {
                if (ok)
                    refreshSnapshots(id);
            });
}

void VmManager::expandDisk(const QString &id, int gibibytes) {
    const auto *item = entry(id);
    if (!item || busy(id) || (session(id) && session(id)->active()) || gibibytes < 1 ||
        gibibytes > 16384) {
        emit errorOccurred("Power off the VM before expanding its virtual disk.");
        return;
    }
    const QString path = diskPath(*item);
    runTool(id, "qemu-img", {"info", "--output=json", path},
            [this, id, path, gibibytes](bool ok, const QByteArray &output) {
                if (!ok)
                    return;
                const qint64 current = QJsonDocument::fromJson(output)
                                           .object()
                                           .value("virtual-size")
                                           .toVariant()
                                           .toLongLong();
                const qint64 requested = qint64(gibibytes) * 1024 * 1024 * 1024;
                if (current <= 0 || requested <= current) {
                    emit errorOccurred("The new capacity must be larger than the actual disk "
                                       "capacity. Disk shrinking is not supported.");
                    return;
                }
                runTool(
                    id, "qemu-img", {"resize", path, QString::number(gibibytes) + "G"},
                    [this, id, gibibytes](bool resized, const QByteArray &) {
                        if (!resized)
                            return;
                        for (auto &item : entries_)
                            if (item.id == id) {
                                item.config.disk_size = std::to_string(gibibytes) + "G";
                                QString error;
                                if (!writeConfig(item.directory + "/wvm.xml", item.config, error))
                                    emit errorOccurred(error);
                            }
                        emit log("Disk expanded. Expand the partition/filesystem inside the guest "
                                 "to use the new capacity.");
                        emit changed();
                    });
            });
}
