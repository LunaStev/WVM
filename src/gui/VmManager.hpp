#pragma once

#include "VMConfig.hpp"
#include <QJsonObject>
#include <QList>
#include <QLocalSocket>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <memory>
class QLockFile;

struct VmEntry {
    QString id;
    QString directory;
    QString family = "Linux";
    QString notes;
    QString configError;
    wvm::VMConfig config;
};

class QmpConnection final : public QObject {
    Q_OBJECT
  public:
    explicit QmpConnection(QObject *parent = nullptr);
    void open(const QString &path);
    void close();
    void command(const QString &name, const QJsonObject &arguments = {});
    bool ready() const { return ready_; }
  signals:
    void readyChanged(bool ready);
    void response(const QString &command, const QJsonObject &result);
    void eventReceived(const QString &event);
    void failure(const QString &message);

  private:
    void readMessages();
    QLocalSocket socket_;
    QByteArray buffer_;
    QMap<int, QString> pending_;
    int sequence_ = 0;
    bool ready_ = false;
};

class VmSession final : public QObject {
    Q_OBJECT
  public:
    explicit VmSession(VmEntry entry, QObject *parent = nullptr);
    ~VmSession() override;
    bool start(QString &error, bool useKvm = true);
    void control(const QString &command);
    bool active() const;
    QString state() const { return state_; }
    QString consolePath() const { return runtime_ + "/console.sock"; }
  signals:
    void changed();
    void log(const QString &message);
    void consoleReady(const QString &path);
    void stopped(bool successful);

  private:
    void setState(const QString &state);
    VmEntry entry_;
    QProcess process_;
    QmpConnection qmp_;
    QTimer poll_;
    QString runtime_;
    QString state_ = "Stopped";
    bool launched_ = false;
    QStringList queuedControls_;
    std::unique_ptr<QLockFile> runtimeLock_;
};

class VmManager final : public QObject {
    Q_OBJECT
  public:
    explicit VmManager(QString storage = {}, QObject *parent = nullptr);
    const QList<VmEntry> &entries() const { return entries_; }
    const VmEntry *entry(const QString &id) const;
    QString importProject(const QString &directory, QString &error);
    void create(VmEntry entry, const QString &sourceDisk = {});
    void clone(const QString &id, const QString &directory, const QString &name);
    bool saveConfiguration(const QString &id, const wvm::VMConfig &config, const QString &family,
                           const QString &notes, QString &error);
    bool removeFromLibrary(const QString &id, QString &error);
    void start(const QString &id);
    void control(const QString &id, const QString &command);
    void snapshot(const QString &id, const QString &operation, const QString &name = {});
    void refreshSnapshots(const QString &id);
    void expandDisk(const QString &id, int gibibytes);
    VmSession *session(const QString &id) const;
    bool busy(const QString &id) const;
    bool anyActive() const;
    bool hasOperations() const { return !operations_.isEmpty(); }
    QString state(const QString &id) const;
    static QString diskPath(const VmEntry &entry);
  signals:
    void changed();
    void errorOccurred(const QString &message);
    void log(const QString &message);
    void selected(const QString &id);
    void snapshotsChanged(const QString &id, const QJsonArray &snapshots);

  private:
    using Completion = std::function<void(bool, const QByteArray &)>;
    void runTool(const QString &id, const QString &program, const QStringList &arguments,
                 Completion completion);
    bool persist(QString &error);
    void load();
    void launch(const QString &id);
    QList<VmEntry> entries_;
    QMap<QString, VmSession *> sessions_;
    QMap<QString, QProcess *> operations_;
    QString storage_;
    bool storageValid_ = true;
};
