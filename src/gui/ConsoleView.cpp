#include "ConsoleView.hpp"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtEndian>
#include <cstring>

namespace {
quint16 read16(const QByteArray &bytes, int offset) {
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}
quint32 read32(const QByteArray &bytes, int offset) {
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}
void append16(QByteArray &bytes, quint16 value) {
    const auto encoded = qToBigEndian(value);
    bytes.append(reinterpret_cast<const char *>(&encoded), sizeof(encoded));
}
void append32(QByteArray &bytes, quint32 value) {
    const auto encoded = qToBigEndian(value);
    bytes.append(reinterpret_cast<const char *>(&encoded), sizeof(encoded));
}
quint32 keySymbol(const QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Escape:
        return 0xff1b;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return 0xff09;
    case Qt::Key_Backspace:
        return 0xff08;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return 0xff0d;
    case Qt::Key_Insert:
        return 0xff63;
    case Qt::Key_Delete:
        return 0xffff;
    case Qt::Key_Home:
        return 0xff50;
    case Qt::Key_End:
        return 0xff57;
    case Qt::Key_PageUp:
        return 0xff55;
    case Qt::Key_PageDown:
        return 0xff56;
    case Qt::Key_Left:
        return 0xff51;
    case Qt::Key_Up:
        return 0xff52;
    case Qt::Key_Right:
        return 0xff53;
    case Qt::Key_Down:
        return 0xff54;
    case Qt::Key_Shift:
        return 0xffe1;
    case Qt::Key_Control:
        return 0xffe3;
    case Qt::Key_Alt:
        return 0xffe9;
    case Qt::Key_AltGr:
        return 0xffea;
    case Qt::Key_Meta:
        return 0xffeb;
    case Qt::Key_CapsLock:
        return 0xffe5;
    case Qt::Key_NumLock:
        return 0xff7f;
    case Qt::Key_ScrollLock:
        return 0xff14;
    case Qt::Key_Print:
        return 0xff61;
    default:
        break;
    }
    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35)
        return 0xffbe + event->key() - Qt::Key_F1;
    if (event->modifiers().testFlag(Qt::ControlModifier) && event->key() >= Qt::Key_A &&
        event->key() <= Qt::Key_Z)
        return event->key() - Qt::Key_A + 'a';
    const auto characters = event->text().toUcs4();
    if (characters.isEmpty())
        return 0;
    return characters.front() <= 0xff ? characters.front() : 0x01000000 | characters.front();
}
quint8 mouseMask(Qt::MouseButtons buttons) {
    return (buttons.testFlag(Qt::LeftButton) ? 1 : 0) |
           (buttons.testFlag(Qt::MiddleButton) ? 2 : 0) |
           (buttons.testFlag(Qt::RightButton) ? 4 : 0);
}
} // namespace

ConsoleView::ConsoleView(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(320, 200);
    retry_.setInterval(500);
    connect(&retry_, &QTimer::timeout, this, [this] {
        if (!path_.isEmpty() && socket_.state() == QLocalSocket::UnconnectedState)
            socket_.connectToServer(path_);
    });
    update_.setInterval(16);
    connect(&update_, &QTimer::timeout, this, [this] {
        if (connected_ && !requested_)
            requestFrame(true);
    });
    connect(&socket_, &QLocalSocket::readyRead, this, [this] {
        buffer_ += socket_.readAll();
        if (buffer_.size() > 128 * 1024 * 1024) {
            fail("Console frame exceeds the supported size.");
            return;
        }
        parse();
    });
    connect(&socket_, &QLocalSocket::connected, this, [this] {
        stage_ = 0;
        rectangles_ = -1;
        buffer_.clear();
        retry_.stop();
    });
    connect(&socket_, &QLocalSocket::disconnected, this, [this] {
        connected_ = false;
        update_.stop();
        pressed_.clear();
        keyMap_.clear();
        message_ = "Console disconnected. Power on the VM to reconnect.";
        emit connectionChanged(false);
        update();
    });
}

void ConsoleView::open(const QString &path) {
    if (path_ == path && socket_.state() == QLocalSocket::ConnectedState)
        return;
    disconnectConsole();
    path_ = path;
    message_ = "Connecting to the virtual machine...";
    socket_.connectToServer(path_);
    retry_.start();
    update();
}

void ConsoleView::disconnectConsole() {
    for (auto symbol : pressed_)
        key(symbol, false);
    pressed_.clear();
    keyMap_.clear();
    socket_.abort();
    retry_.stop();
    update_.stop();
    path_.clear();
    buffer_.clear();
    frame_ = QImage();
    connected_ = false;
    requested_ = false;
    buttons_ = 0;
    message_ = "Power on a virtual machine to open its console.";
    update();
}

void ConsoleView::fail(const QString &message) {
    disconnectConsole();
    message_ = message;
    update();
}

void ConsoleView::parse() {
    for (;;) {
        if (stage_ == 0) {
            if (buffer_.size() < 12)
                return;
            if (!buffer_.startsWith("RFB 003.")) {
                fail("Unsupported console protocol.");
                return;
            }
            buffer_.remove(0, 12);
            socket_.write("RFB 003.008\n", 12);
            stage_ = 1;
        } else if (stage_ == 1) {
            if (buffer_.isEmpty())
                return;
            const int count = static_cast<quint8>(buffer_[0]);
            if (count == 0) {
                fail("The VM rejected the console connection.");
                return;
            }
            if (buffer_.size() < count + 1)
                return;
            if (!buffer_.mid(1, count).contains(char(1))) {
                fail("Unsupported console authentication.");
                return;
            }
            buffer_.remove(0, count + 1);
            socket_.write(QByteArray(1, char(1)));
            stage_ = 2;
        } else if (stage_ == 2) {
            if (buffer_.size() < 4)
                return;
            if (read32(buffer_, 0) != 0) {
                fail("Console authorization failed.");
                return;
            }
            buffer_.remove(0, 4);
            socket_.write(QByteArray(1, char(1))); // shared local client
            stage_ = 3;
        } else if (stage_ == 3) {
            if (buffer_.size() < 24)
                return;
            const quint32 titleLength = read32(buffer_, 20);
            if (titleLength > 1024 * 1024) {
                fail("Invalid console title.");
                return;
            }
            if (buffer_.size() < 24 + titleLength)
                return;
            const int width = read16(buffer_, 0), height = read16(buffer_, 2);
            if (width < 1 || height < 1 || width > 8192 || height > 8192 ||
                qint64(width) * height > 16 * 1024 * 1024) {
                fail("Unsupported console resolution.");
                return;
            }
            frame_ = QImage(width, height, QImage::Format_RGB32);
            frame_.fill(Qt::black);
            buffer_.remove(0, 24 + titleLength);
            QByteArray format(4, 0);
            format.append(char(32));
            format.append(char(24));
            format.append(char(0));
            format.append(char(1));
            append16(format, 255);
            append16(format, 255);
            append16(format, 255);
            format.append(char(16));
            format.append(char(8));
            format.append(char(0));
            format.append(QByteArray(3, 0));
            socket_.write(format);
            QByteArray encodings;
            encodings.append(char(2));
            encodings.append(char(0));
            append16(encodings, 3);
            append32(encodings, 0);
            append32(encodings, 1);
            append32(encodings, quint32(-223));
            socket_.write(encodings);
            stage_ = 4;
            connected_ = true;
            requestFrame(false);
            update_.start();
            emit connectionChanged(true);
        } else {
            if (rectangles_ < 0) {
                if (buffer_.isEmpty())
                    return;
                const quint8 type = buffer_[0];
                if (type == 0) {
                    if (buffer_.size() < 4)
                        return;
                    rectangles_ = read16(buffer_, 2);
                    buffer_.remove(0, 4);
                } else if (type == 2) {
                    buffer_.remove(0, 1);
                    continue;
                } else if (type == 3) {
                    if (buffer_.size() < 8)
                        return;
                    const quint32 size = read32(buffer_, 4);
                    if (size > 4 * 1024 * 1024) {
                        fail("Console clipboard exceeds the supported size.");
                        return;
                    }
                    if (buffer_.size() < 8 + size)
                        return;
                    buffer_.remove(0, 8 + size);
                    continue;
                } else {
                    fail("Unsupported console message.");
                    return;
                }
            }
            if (rectangles_ == 0) {
                rectangles_ = -1;
                requested_ = false;
                update();
                emit frameReceived();
                continue;
            }
            if (buffer_.size() < 12)
                return;
            const int x = read16(buffer_, 0), y = read16(buffer_, 2);
            const int width = read16(buffer_, 4), height = read16(buffer_, 6);
            const qint32 encoding = static_cast<qint32>(read32(buffer_, 8));
            if (encoding == -223) {
                if (width < 1 || height < 1 || width > 8192 || height > 8192 ||
                    qint64(width) * height > 16 * 1024 * 1024) {
                    fail("Invalid console resize.");
                    return;
                }
                frame_ = QImage(width, height, QImage::Format_RGB32);
                frame_.fill(Qt::black);
                buffer_.remove(0, 12);
                --rectangles_;
                continue;
            }
            if (x + width > frame_.width() || y + height > frame_.height()) {
                fail("Invalid framebuffer rectangle.");
                return;
            }
            if (encoding == 0) {
                const qint64 payload = qint64(width) * height * 4;
                if (buffer_.size() < 12 + payload)
                    return;
                for (int row = 0; row < height; ++row) {
                    auto *pixels = reinterpret_cast<QRgb *>(frame_.scanLine(y + row));
                    const auto *source =
                        reinterpret_cast<const uchar *>(buffer_.constData() + 12 + row * width * 4);
                    for (int column = 0; column < width; ++column)
                        pixels[x + column] = qRgb(source[column * 4 + 2], source[column * 4 + 1],
                                                  source[column * 4]);
                }
                buffer_.remove(0, 12 + payload);
            } else if (encoding == 1) {
                if (buffer_.size() < 16)
                    return;
                const int sx = read16(buffer_, 12), sy = read16(buffer_, 14);
                if (sx + width > frame_.width() || sy + height > frame_.height()) {
                    fail("Invalid console copy rectangle.");
                    return;
                }
                const auto copy = frame_.copy(sx, sy, width, height);
                QPainter painter(&frame_);
                painter.drawImage(x, y, copy);
                buffer_.remove(0, 16);
            } else {
                fail("Unsupported console encoding.");
                return;
            }
            --rectangles_;
        }
    }
}

void ConsoleView::requestFrame(bool incremental) {
    if (!connected_)
        return;
    QByteArray request;
    request.append(char(3));
    request.append(char(incremental ? 1 : 0));
    append16(request, 0);
    append16(request, 0);
    append16(request, frame_.width());
    append16(request, frame_.height());
    socket_.write(request);
    requested_ = true;
}

QRect ConsoleView::frameRect() const {
    if (frame_.isNull())
        return {};
    const QSize size = frame_.size().scaled(this->size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size);
}
void ConsoleView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (connected_ && !frame_.isNull())
        painter.drawImage(frameRect(), frame_);
    else {
        painter.setPen(Qt::lightGray);
        painter.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap,
                         message_);
    }
}
void ConsoleView::pointer(const QPointF &position, quint8 mask) {
    const QRect area = frameRect();
    if (!connected_ || area.isEmpty())
        return;
    const int x = qBound(0, int((position.x() - area.x()) * frame_.width() / area.width()),
                         frame_.width() - 1);
    const int y = qBound(0, int((position.y() - area.y()) * frame_.height() / area.height()),
                         frame_.height() - 1);
    QByteArray data;
    data.append(char(5));
    data.append(char(mask));
    append16(data, x);
    append16(data, y);
    socket_.write(data);
}
void ConsoleView::mouseMoveEvent(QMouseEvent *event) { pointer(event->position(), buttons_); }
void ConsoleView::mousePressEvent(QMouseEvent *event) {
    setFocus();
    buttons_ = mouseMask(event->buttons());
    pointer(event->position(), buttons_);
}
void ConsoleView::mouseReleaseEvent(QMouseEvent *event) {
    buttons_ = mouseMask(event->buttons());
    pointer(event->position(), buttons_);
}
void ConsoleView::wheelEvent(QWheelEvent *event) {
    pointer(event->position(), buttons_ | (event->angleDelta().y() > 0 ? 8 : 16));
    pointer(event->position(), buttons_);
}
void ConsoleView::key(quint32 symbol, bool down) {
    if (!connected_ || symbol == 0)
        return;
    QByteArray data;
    data.append(char(4));
    data.append(char(down ? 1 : 0));
    data.append(QByteArray(2, 0));
    append32(data, symbol);
    socket_.write(data);
}
void ConsoleView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Alt && event->modifiers().testFlag(Qt::ControlModifier)) {
        clearFocus();
        event->accept();
        return;
    }
    const quint32 symbol = keySymbol(event);
    if (symbol) {
        pressed_.insert(symbol);
        keyMap_.insert(event->key(), symbol);
        key(symbol, true);
    }
    event->accept();
}
void ConsoleView::keyReleaseEvent(QKeyEvent *event) {
    if (!event->isAutoRepeat()) {
        const auto symbol = keyMap_.take(event->key());
        pressed_.remove(symbol);
        key(symbol, false);
    }
    event->accept();
}
void ConsoleView::focusOutEvent(QFocusEvent *event) {
    for (auto symbol : pressed_)
        key(symbol, false);
    pressed_.clear();
    keyMap_.clear();
    pointer(mapFromGlobal(QCursor::pos()), 0);
    buttons_ = 0;
    QWidget::focusOutEvent(event);
}
void ConsoleView::sendCtrlAltDelete() {
    key(0xffe3, true);
    key(0xffe9, true);
    key(0xffff, true);
    key(0xffff, false);
    key(0xffe9, false);
    key(0xffe3, false);
}
