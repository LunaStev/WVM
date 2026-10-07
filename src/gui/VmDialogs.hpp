#pragma once
#include "VmManager.hpp"
#include <QDialog>
#include <QWizard>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QPlainTextEdit;
class NewVmWizard final : public QWizard {
  public:
    explicit NewVmWizard(QWidget *parent = nullptr);
    VmEntry result() const;
    QString sourceDisk() const;
    bool validateCurrentPage() override;

  private:
    QLineEdit *name_;
    QLineEdit *directory_;
    QComboBox *family_;
    QSpinBox *cores_;
    QSpinBox *memory_;
    QSpinBox *disk_;
    QLineEdit *iso_;
    QLineEdit *source_;
};

class VmSettingsDialog final : public QDialog {
  public:
    explicit VmSettingsDialog(const VmEntry &entry, QWidget *parent = nullptr);
    wvm::VMConfig configuration() const;
    QString family() const;
    QString notes() const;

  private:
    wvm::VMConfig original_;
    QLineEdit *name_;
    QComboBox *family_;
    QSpinBox *cores_;
    QComboBox *memory_;
    QComboBox *network_;
    QComboBox *networkModel_;
    QComboBox *diskInterface_;
    QComboBox *graphics_;
    QLineEdit *media_;
    QPlainTextEdit *notes_;
};
