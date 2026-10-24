/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BLUEZPROFILE_H
#define BLUEZPROFILE_H

#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QVariantMap>

/*
 * org.bluez.Profile1 client for the Nothing control service. After
 * registration, Device1.ConnectProfile(uuid) makes BlueZ look up the RFCOMM
 * channel via SDP, connect, and hand the socket to NewConnection().
 */
class BluezProfile : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Profile1")

public:
    explicit BluezProfile(QObject *parent = nullptr);

    // Exports this object and registers the profile with BlueZ.
    // Safe to call repeatedly; returns true once registered.
    bool ensureRegistered();
    QString lastError() const { return m_lastError; }

    static QString objectPath();

public slots:
    // Called by BlueZ over D-Bus.
    void Release();
    void NewConnection(const QDBusObjectPath &device, const QDBusUnixFileDescriptor &fd,
                       const QVariantMap &properties);
    void RequestDisconnection(const QDBusObjectPath &device);
    void Cancel();

signals:
    void connectionReady(const QString &devicePath, int fd);
    void disconnectionRequested(const QString &devicePath);

private:
    bool m_exported;
    bool m_registered;
    QString m_lastError;
};

#endif // BLUEZPROFILE_H
