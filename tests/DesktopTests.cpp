#include "ConsoleView.hpp"
#include "VmDialogs.hpp"
#include "VmManager.hpp"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class DesktopTests final : public QObject {
    Q_OBJECT
  private slots:
    void malformedLibraryIsPreserved() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString storage = temporary.path() + "/library.json";
        const QByteArray damaged = "{\"version\":2,\"machines\":false}";
        QFile file(storage);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(damaged), damaged.size());
        file.close();
        QVERIFY(wvm::save_config(
            std::filesystem::path((temporary.path() + "/wvm.xml").toStdString()), wvm::VMConfig{}));
        VmManager manager(storage);
        QString error;
        QVERIFY(manager.importProject(temporary.path(), error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), damaged);
    }
    void librarySnapshotsAndClone() {
        if (QStandardPaths::findExecutable("qemu-img").isEmpty())
            QSKIP("qemu-img is not installed");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString storage = temporary.path() + "/library.json";
        VmManager manager(storage);
        QSignalSpy selected(&manager, &VmManager::selected);
        QSignalSpy errors(&manager, &VmManager::errorOccurred);
        VmEntry entry;
        entry.directory = temporary.path() + "/original";
        entry.config.name = "Test VM";
        entry.config.disk_size = "16M";
        entry.config.boot_mode = "disk";
        manager.create(entry);
        QTRY_COMPARE_WITH_TIMEOUT(selected.size(), 1, 10000);
        QCOMPARE(errors.size(), 0);
        const QString id = selected.front().front().toString();
        QVERIFY(manager.entry(id));
        QVERIFY(QFileInfo::exists(VmManager::diskPath(*manager.entry(id))));
        QString error;
        QCOMPARE(manager.importProject(entry.directory, error), id);
        QCOMPARE(manager.entries().size(), 1);
        auto config = manager.entry(id)->config;
        config.memory = "2G";
        QVERIFY(manager.saveConfiguration(id, config, "BSD", "Persistent notes", error));
        VmManager restored(storage);
        QCOMPARE(restored.entries().size(), 1);
        QCOMPARE(restored.entry(id)->family, QString("BSD"));
        QCOMPARE(restored.entry(id)->notes, QString("Persistent notes"));
        QCOMPARE(restored.entry(id)->config.memory, std::string("2G"));

        QSignalSpy snapshots(&manager, &VmManager::snapshotsChanged);
        auto writePattern = [&](const QString &pattern) {
            QProcess write;
            write.start("qemu-io", {"-f", "qcow2", "-c", "write -P " + pattern + " 0 4096",
                                    VmManager::diskPath(*manager.entry(id))});
            return write.waitForFinished() && write.exitCode() == 0;
        };
        const bool checkContents = !QStandardPaths::findExecutable("qemu-io").isEmpty();
        if (checkContents)
            QVERIFY(writePattern("0x11"));
        manager.snapshot(id, "-c", "clean-install");
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 10000);
        QCOMPARE(snapshots.last()[1].toJsonArray().size(), 1);
        if (checkContents)
            QVERIFY(writePattern("0x22"));
        manager.snapshot(id, "-a", "clean-install");
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 2, 10000);
        if (checkContents) {
            QProcess read;
            read.start("qemu-io", {"-f", "qcow2", "-c", "read -P 0x11 0 4096",
                                   VmManager::diskPath(*manager.entry(id))});
            QVERIFY(read.waitForFinished());
            QCOMPARE(read.exitCode(), 0);
        }
        manager.snapshot(id, "-d", "clean-install");
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 3, 10000);
        QVERIFY(snapshots.last()[1].toJsonArray().isEmpty());
        manager.expandDisk(id, 1);
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(id), 10000);
        QCOMPARE(manager.entry(id)->config.disk_size, std::string("1G"));
        manager.expandDisk(id, 1);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 10000);
        manager.clone(id, temporary.path() + "/clone", "Cloned VM");
        QTRY_COMPARE_WITH_TIMEOUT(selected.size(), 2, 10000);
        QCOMPARE(manager.entries().size(), 2);
        const QString cloneId = selected.last().first().toString();
        QVERIFY(manager.entry(cloneId));
        QCOMPARE(manager.entry(cloneId)->config.name, std::string("Cloned VM"));
        QVERIFY(manager.removeFromLibrary(cloneId, error));
        QVERIFY(QFileInfo::exists(temporary.path() + "/clone/disk.qcow2"));
        // Creation must never overwrite an existing VM directory.
        manager.create(entry);
        QCOMPARE(errors.size(), 2);
        QVERIFY(QFileInfo::exists(entry.directory + "/wvm.xml"));
        const QString artifacts = qEnvironmentVariable("WVM_TEST_ARTIFACT_DIR");
        if (!artifacts.isEmpty()) {
            QDir().mkpath(artifacts);
            NewVmWizard wizard;
            wizard.show();
            QTest::qWait(50);
            QVERIFY(wizard.grab().save(artifacts + "/new-vm-wizard.png"));
            VmSettingsDialog settings(*manager.entry(id));
            settings.show();
            QTest::qWait(50);
            QVERIFY(settings.grab().save(artifacts + "/vm-settings.png"));
        }
    }

    void actualQemuConsoleAndLifecycle() {
        if (QStandardPaths::findExecutable("qemu-system-x86_64").isEmpty() ||
            QStandardPaths::findExecutable("qemu-img").isEmpty())
            QSKIP("QEMU is not installed");
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QProcess disk;
        qputenv("XDG_RUNTIME_DIR", temporary.path().toUtf8());
        disk.start("qemu-img", {"create", "-f", "qcow2", temporary.path() + "/disk.qcow2", "16M"});
        QVERIFY(disk.waitForFinished());
        QCOMPARE(disk.exitCode(), 0);
        VmEntry entry;
        entry.directory = temporary.path();
        entry.config.memory = "128M";
        entry.config.cores = 1;
        entry.config.disk_size = "16M";
        entry.config.boot_mode = "disk";
        VmSession session(entry);
        QString error;
        QSignalSpy logs(&session, &VmSession::log);
        connect(&session, &VmSession::log, this,
                [](const QString &message) { qInfo().noquote() << message; });
        QVERIFY2(session.start(error, false),
                 qPrintable(error)); // TCG only in this integration test.
        QTRY_VERIFY_WITH_TIMEOUT(session.state() == "Running" || session.state() == "Error", 10000);
        QCOMPARE(session.state(), QString("Running"));
        VmEntry secondEntry = entry;
        secondEntry.directory = temporary.path() + "/second";
        QVERIFY(QDir().mkpath(secondEntry.directory));
        disk.start("qemu-img",
                   {"create", "-f", "qcow2", secondEntry.directory + "/disk.qcow2", "16M"});
        QVERIFY(disk.waitForFinished());
        QCOMPARE(disk.exitCode(), 0);
        VmSession second(secondEntry);
        QVERIFY2(second.start(error, false), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(second.state() == "Running" || second.state() == "Error", 10000);
        QCOMPARE(second.state(), QString("Running"));
        VmSession duplicate(entry);
        QVERIFY(!duplicate.start(error, false));
        QVERIFY(error.contains("another"));
        ConsoleView console;
        QSignalSpy frames(&console, &ConsoleView::frameReceived);
        console.open(session.consolePath());
        QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty(), 10000);
        QVERIFY(!console.framebuffer().isNull());
        QVERIFY(console.framebuffer().width() >= 320);
        const QString artifacts = qEnvironmentVariable("WVM_TEST_ARTIFACT_DIR");
        if (!artifacts.isEmpty()) {
            QDir().mkpath(artifacts);
            QVERIFY(console.framebuffer().save(artifacts + "/guest-console.png"));
        }
        console.sendCtrlAltDelete();
        session.control("stop");
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), QString("Paused"), 5000);
        QCOMPARE(second.state(), QString("Running"));
        session.control("cont");
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), QString("Running"), 5000);
        session.control("quit");
        QTRY_VERIFY_WITH_TIMEOUT(!session.active(), 5000);
        QCOMPARE(session.state(), QString("Stopped"));
        QVERIFY(!QFileInfo::exists(session.consolePath()));
        second.control("quit");
        QTRY_VERIFY_WITH_TIMEOUT(!second.active(), 5000);
    }
};
QTEST_MAIN(DesktopTests)
#include "DesktopTests.moc"
