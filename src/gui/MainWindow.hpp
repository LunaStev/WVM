#pragma once
#include "VmManager.hpp"
#include <QMainWindow>

class QAction;
class ConsoleView;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTabWidget;
class QTableWidget;
class QTreeWidget;

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    void openProject(const QString &path);

  protected:
    void closeEvent(QCloseEvent *event) override;

  private:
    void rebuildLibrary();
    void select(const QString &id);
    void updateSelection();
    void newMachine();
    void settings();
    void cloneMachine();
    void removeMachine();
    void snapshot(const QString &operation);
    void appendLog(const QString &message);
    void hostInformation();
    VmManager manager_;
    QString selected_;
    QString connectedConsole_;
    bool closing_ = false;
    QTreeWidget *library_;
    QLineEdit *search_;
    QTabWidget *tabs_;
    QTableWidget *overview_;
    QTableWidget *hardware_;
    QTableWidget *snapshots_;
    QPlainTextEdit *notes_;
    QPlainTextEdit *log_;
    ConsoleView *console_;
    QLabel *name_;
    QLabel *state_;
    QLabel *location_;
    QAction *start_;
    QAction *shutdown_;
    QAction *stop_;
    QAction *pause_;
    QAction *reset_;
    QAction *settings_;
    QAction *clone_;
    QAction *remove_;
    QAction *snapshotCreate_;
    QAction *snapshotRestore_;
    QAction *snapshotDelete_;
};
