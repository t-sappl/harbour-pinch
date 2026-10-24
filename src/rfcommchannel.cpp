/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "rfcommchannel.h"

#include <QDebug>
#include <QSocketNotifier>
#include <QStringList>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

// Kernel ABI values, so the BlueZ development headers are not required.
#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif
#ifndef BTPROTO_RFCOMM
#define BTPROTO_RFCOMM 3
#endif

namespace {
// Same layout as struct sockaddr_rc in <bluetooth/rfcomm.h>.
struct SockAddrRc {
    sa_family_t family;
    uint8_t bdaddr[6]; // little endian, i.e. reversed compared to the text form
    uint8_t channel;
};

bool parseAddress(const QString &text, uint8_t out[6])
{
    const QStringList parts = text.split(QLatin1Char(':'));
    if (parts.size() != 6)
        return false;
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        const uint value = parts.at(i).toUInt(&ok, 16);
        if (!ok || value > 0xFF)
            return false;
        out[5 - i] = static_cast<uint8_t>(value);
    }
    return true;
}

void setNonBlocking(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
}

RfcommChannel::RfcommChannel(QObject *parent)
    : QObject(parent)
    , m_fd(-1)
    , m_connected(false)
    , m_readNotifier(nullptr)
    , m_writeNotifier(nullptr)
{
}

RfcommChannel::~RfcommChannel()
{
    cleanup();
}

void RfcommChannel::adopt(int fd)
{
    cleanup();
    m_fd = fd;
    setNonBlocking(m_fd);
    m_connected = true;
    startReading();
}

void RfcommChannel::connectToChannel(const QString &address, int channel)
{
    cleanup();

    SockAddrRc addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.family = AF_BLUETOOTH;
    addr.channel = static_cast<uint8_t>(channel);
    if (!parseAddress(address, addr.bdaddr)) {
        emit failed(QStringLiteral("invalid address %1").arg(address));
        return;
    }

    m_fd = ::socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (m_fd < 0) {
        emit failed(QString::fromLocal8Bit(std::strerror(errno)));
        return;
    }
    setNonBlocking(m_fd);

    if (::connect(m_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0) {
        m_connected = true;
        startReading();
        emit connected();
        return;
    }
    if (errno != EINPROGRESS) {
        const QString error = QString::fromLocal8Bit(std::strerror(errno));
        cleanup();
        emit failed(error);
        return;
    }

    m_writeNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, &RfcommChannel::onWritable);
}

void RfcommChannel::onWritable()
{
    m_writeNotifier->setEnabled(false);
    m_writeNotifier->deleteLater();
    m_writeNotifier = nullptr;

    int error = 0;
    socklen_t length = sizeof(error);
    if (::getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
        error = errno;

    if (error != 0) {
        cleanup();
        emit failed(QString::fromLocal8Bit(std::strerror(error)));
        return;
    }

    m_connected = true;
    startReading();
    emit connected();
}

void RfcommChannel::startReading()
{
    m_readNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &RfcommChannel::onReadable);
}

void RfcommChannel::onReadable()
{
    char buffer[512];
    for (;;) {
        const ssize_t n = ::read(m_fd, buffer, sizeof(buffer));
        if (n > 0) {
            emit dataReceived(QByteArray(buffer, int(n)));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        if (n < 0 && errno == EINTR)
            continue;
        // n == 0 (remote closed) or a real error
        qDebug() << "RFCOMM channel closed" << (n < 0 ? std::strerror(errno) : "");
        cleanup();
        emit closed();
        return;
    }
}

bool RfcommChannel::write(const QByteArray &data)
{
    if (!isOpen())
        return false;
    // Packets are tiny (< 20 bytes), a single write normally takes all of it.
    const ssize_t n = ::write(m_fd, data.constData(), size_t(data.size()));
    if (n != data.size()) {
        qWarning() << "RFCOMM write failed or was short:" << n << std::strerror(errno);
        return false;
    }
    return true;
}

void RfcommChannel::close()
{
    cleanup();
}

void RfcommChannel::cleanup()
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        m_writeNotifier->deleteLater();
        m_writeNotifier = nullptr;
    }
    if (m_fd >= 0) {
        ::shutdown(m_fd, SHUT_RDWR);
        ::close(m_fd);
        m_fd = -1;
    }
    m_connected = false;
}
