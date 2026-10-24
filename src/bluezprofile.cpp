/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bluezprofile.h"
#include "nothingprotocol.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>

#include <unistd.h>

BluezProfile::BluezProfile(QObject *parent)
    : QObject(parent)
    , m_exported(false)
    , m_registered(false)
{
}

QString BluezProfile::objectPath()
{
    return QStringLiteral("/org/sappl/pinch/profile");
}

bool BluezProfile::ensureRegistered()
{
    if (m_registered)
        return true;

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!(bus.connectionCapabilities() & QDBusConnection::UnixFileDescriptorPassing)) {
        m_lastError = QStringLiteral("D-Bus connection cannot pass file descriptors");
        qWarning() << m_lastError;
        return false;
    }

    if (!m_exported) {
        if (!bus.registerObject(objectPath(), this, QDBusConnection::ExportAllSlots)) {
            m_lastError = bus.lastError().message();
            qWarning() << "Exporting profile object failed:" << m_lastError;
            return false;
        }
        m_exported = true;
    }

    QVariantMap options;
    options.insert(QStringLiteral("Name"), QStringLiteral("Pinch"));
    options.insert(QStringLiteral("Role"), QStringLiteral("client"));
    options.insert(QStringLiteral("AutoConnect"), false);
    options.insert(QStringLiteral("RequireAuthentication"), false);
    options.insert(QStringLiteral("RequireAuthorization"), false);

    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"),
                                                       QStringLiteral("/org/bluez"),
                                                       QStringLiteral("org.bluez.ProfileManager1"),
                                                       QStringLiteral("RegisterProfile"));
    call << QVariant::fromValue(QDBusObjectPath(objectPath()))
         << QString::fromLatin1(nothing::SppServiceUuid)
         << options;

    QDBusReply<void> reply = bus.call(call);
    if (!reply.isValid()
        && reply.error().name() != QLatin1String("org.bluez.Error.AlreadyExists")) {
        m_lastError = reply.error().message();
        qWarning() << "RegisterProfile failed:" << reply.error().name() << m_lastError;
        return false;
    }

    if (!reply.isValid())
        qWarning() << "Profile UUID already registered - another app (e.g. Sailbuds) may receive the connection";
    else
        qDebug() << "Profile registered";
    m_registered = true;
    m_lastError.clear();
    return true;
}

void BluezProfile::Release()
{
    qDebug() << "Profile released by BlueZ";
    m_registered = false;
}

void BluezProfile::NewConnection(const QDBusObjectPath &device, const QDBusUnixFileDescriptor &fd,
                                 const QVariantMap &properties)
{
    Q_UNUSED(properties)
    // The descriptor object closes its copy when it goes away, so keep our own.
    const int ownFd = ::dup(fd.fileDescriptor());
    qDebug() << "NewConnection from" << device.path() << "fd" << ownFd;
    if (ownFd < 0)
        return;
    emit connectionReady(device.path(), ownFd);
}

void BluezProfile::RequestDisconnection(const QDBusObjectPath &device)
{
    qDebug() << "RequestDisconnection" << device.path();
    emit disconnectionRequested(device.path());
}

void BluezProfile::Cancel()
{
    qDebug() << "Profile request cancelled";
}
