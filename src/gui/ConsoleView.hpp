#pragma once
#include <QImage>
#include <QLocalSocket>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QWidget>

// RFB 3.8 over the VM's private Unix socket. No network port is exposed.
class ConsoleView final : public QWidget {
    Q_OBJECT
  public:
    explicit ConsoleView(QWidget *parent = nullptr);
    void open(const QString &path);
    void disconnectConsole();
    void sendCtrlAltDelete();
    QImage framebuffer() const { return frame_; }
  signals:
    void frameReceived();
    void connectionChanged(bool connected);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void focusOutEvent(QFocusEvent *) override;

  private:
    void parse();
    void requestFrame(bool incremental);
    void key(quint32 symbol, bool down);
    void pointer(const QPointF &position, quint8 mask);
    QRect frameRect() const;
    void fail(const QString &message);
    QLocalSocket socket_;
    QTimer retry_;
    QTimer update_;
    QString path_;
    QString message_ = "Power on a virtual machine to open its console.";
    QByteArray buffer_;
    QImage frame_;
    QSet<quint32> pressed_;
    QMap<int, quint32> keyMap_;
    int stage_ = 0;
    int rectangles_ = -1;
    bool connected_ = false;
    bool requested_ = false;
    quint8 buttons_ = 0;
};
