/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "devicemodel.h"
#include "formfactor.h"
#include "nothingprotocol.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <algorithm>

namespace {
const char *const BluezService = "org.bluez";
const char *const DeviceInterface = "org.bluez.Device1";
}

DeviceModel::DeviceModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_available(false)
{
    qDBusRegisterMetaType<InterfaceList>();
    qDBusRegisterMetaType<ManagedObjectList>();

    QDBusConnection bus = QDBusConnection::systemBus();
    // Empty path matches every object, i.e. every device below the adapter.
    bus.connect(BluezService, QString(), "org.freedesktop.DBus.Properties", "PropertiesChanged",
                this, SLOT(onPropertiesChanged(QDBusMessage)));
    bus.connect(BluezService, "/", "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
                this, SLOT(onInterfacesChanged()));
    bus.connect(BluezService, "/", "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
                this, SLOT(onInterfacesChanged()));
}

int DeviceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_devices.count();
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.count())
        return QVariant();

    const Device &d = m_devices.at(index.row());
    switch (role) {
    case NameRole:      return d.name;
    case AddressRole:   return d.address;
    case ConnectedRole: return d.connected;
    case NothingRole:   return d.nothing;
    case PathRole:      return d.path;
    case ModelNameRole: return d.modelName;
    case FormFactorRole: return int(formfactor::fromName(d.modelName));
    default:            return QVariant();
    }
}

QHash<int, QByteArray> DeviceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[AddressRole] = "address";
    roles[ConnectedRole] = "connected";
    roles[NothingRole] = "isNothing";
    roles[PathRole] = "path";
    roles[ModelNameRole] = "modelName";
    roles[FormFactorRole] = "formFactor";
    return roles;
}

void DeviceModel::refresh()
{
    QDBusMessage call = QDBusMessage::createMethodCall(BluezService, "/",
                                                       "org.freedesktop.DBus.ObjectManager",
                                                       "GetManagedObjects");
    QDBusPendingCallWatcher *watcher =
        new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<ManagedObjectList> reply = *w;

        const bool wasAvailable = m_available;
        m_available = !reply.isError();
        if (m_available != wasAvailable)
            emit availableChanged();

        if (reply.isError()) {
            qWarning() << "BlueZ not reachable:" << reply.error().message();
            return;
        }

        QList<Device> devices;
        const ManagedObjectList objects = reply.value();
        for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
            if (!it.value().contains(DeviceInterface))
                continue;
            const QVariantMap props = it.value().value(DeviceInterface);
            if (!props.value("Paired").toBool())
                continue;

            Device d;
            d.path = it.key().path();
            d.address = props.value("Address").toString();
            d.name = props.value("Alias").toString();
            d.modelName = props.value("Name").toString();
            if (d.name.isEmpty())
                d.name = d.modelName;
            if (d.name.isEmpty())
                d.name = d.address;
            if (d.modelName.isEmpty())
                d.modelName = d.name;
            d.connected = props.value("Connected").toBool();
            const QStringList uuids = props.value("UUIDs").toStringList();
            d.nothing = looksLikeNothing(d.modelName, uuids);
            // Only audio devices: keyboards, mice, watches etc. stay out.
            if (!d.nothing && !looksLikeAudio(props.value("Icon").toString(),
                                              props.value("Class").toUInt(), uuids))
                continue;
            devices.append(d);
        }

        std::stable_sort(devices.begin(), devices.end(), [](const Device &a, const Device &b) {
            if (a.nothing != b.nothing)
                return a.nothing;
            if (a.connected != b.connected)
                return a.connected;
            return a.name.localeAwareCompare(b.name) < 0;
        });

        const int oldCount = m_devices.count();
        beginResetModel();
        m_devices = devices;
        endResetModel();
        if (oldCount != m_devices.count())
            emit countChanged();

        // Report connection changes after the model is consistent.
        for (const Device &d : m_devices) {
            const bool known = m_knownConnected.value(d.address, false);
            if (known != d.connected || !m_knownConnected.contains(d.address)) {
                m_knownConnected.insert(d.address, d.connected);
                qDebug() << "Device" << d.address << d.modelName << (d.connected ? "connected" : "not connected");
                emit systemConnectionChanged(d.address, d.path, d.modelName, d.connected);
            }
        }
    });
}

void DeviceModel::onPropertiesChanged(const QDBusMessage &message)
{
    const QList<QVariant> args = message.arguments();
    if (args.count() < 2 || args.at(0).toString() != QLatin1String(DeviceInterface))
        return;

    const QVariantMap changed = qdbus_cast<QVariantMap>(args.at(1));
    static const QStringList relevant{ "Connected", "Paired", "Alias", "Name", "UUIDs" };
    for (const QString &key : relevant) {
        if (changed.contains(key)) {
            refresh();
            return;
        }
    }
}

void DeviceModel::onInterfacesChanged()
{
    refresh();
}

bool DeviceModel::looksLikeNothing(const QString &name, const QStringList &uuids)
{
    const QString uuid = QString::fromLatin1(nothing::SppServiceUuid);
    for (const QString &u : uuids) {
        if (u.compare(uuid, Qt::CaseInsensitive) == 0)
            return true;
    }
    return name.startsWith(QLatin1String("Nothing"), Qt::CaseInsensitive)
        || name.startsWith(QLatin1String("CMF"), Qt::CaseInsensitive);
}

bool DeviceModel::looksLikeAudio(const QString &icon, uint deviceClass, const QStringList &uuids)
{
    if (icon.startsWith(QLatin1String("audio"), Qt::CaseInsensitive))
        return true;
    // Class of Device: major device class in bits 8-12, 0x04 = audio/video.
    if (deviceClass != 0)
        return ((deviceClass >> 8) & 0x1F) == 0x04;
    // No class known: look for audio services (A2DP sink, headset, hands-free).
    static const char *const audioUuids[] = {
        "0000110b-0000-1000-8000-00805f9b34fb", // A2DP sink
        "00001108-0000-1000-8000-00805f9b34fb", // headset
        "0000111e-0000-1000-8000-00805f9b34fb", // hands-free
    };
    for (const QString &u : uuids) {
        for (const char *a : audioUuids) {
            if (u.compare(QLatin1String(a), Qt::CaseInsensitive) == 0)
                return true;
        }
    }
    return false;
}
