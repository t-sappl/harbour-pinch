/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RFCOMMCHANNEL_H
#define RFCOMMCHANNEL_H

#include <QByteArray>
#include <QObject>
#include <QString>

class QSocketNotifier;

/*
 * Minimal wrapper around a connected RFCOMM stream socket. It either adopts
 * a descriptor that BlueZ handed over (Profile1.NewConnection) or connects
 * to a fixed channel itself. No QtBluetooth needed: the Sailfish SDK does
 * not ship its development files.
 */
class RfcommChannel : public QObject
{
    Q_OBJECT

public:
    explicit RfcommChannel(QObject *parent = nullptr);
    ~RfcommChannel();

    // Takes ownership of an already connected descriptor.
    void adopt(int fd);

    // Non-blocking connect to 'address' (AA:BB:CC:DD:EE:FF) on 'channel'.
    // Emits connected() or failed().
    void connectToChannel(const QString &address, int channel);

    bool isOpen() const { return m_fd >= 0 && m_connected; }
    bool write(const QByteArray &data);
    void close();

signals:
    void connected();
    void failed(const QString &error);
    void dataReceived(const QByteArray &data);
    void closed();

private slots:
    void onReadable();
    void onWritable();

private:
    void startReading();
    void cleanup();

    int m_fd;
    bool m_connected;
    QSocketNotifier *m_readNotifier;
    QSocketNotifier *m_writeNotifier;
};

#endif // RFCOMMCHANNEL_H
