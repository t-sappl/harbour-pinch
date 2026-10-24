/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include <QAbstractListModel>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QList>
#include <QMap>
#include <QVariantMap>

typedef QMap<QString, QVariantMap> InterfaceList;
typedef QMap<QDBusObjectPath, InterfaceList> ManagedObjectList;
Q_DECLARE_METATYPE(InterfaceList)
Q_DECLARE_METATYPE(ManagedObjectList)

/*
 * Paired Bluetooth devices as reported by BlueZ 5 over D-Bus. Devices that
 * advertise the Nothing control service (or look like Nothing/CMF buds by
 * name) are sorted first and flagged, so the user rarely has to search.
 */
class DeviceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AddressRole,
        ConnectedRole,
        NothingRole,
        PathRole,
        ModelNameRole,
        FormFactorRole
    };

    explicit DeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool available() const { return m_available; }

    Q_INVOKABLE void refresh();

signals:
    void countChanged();
    void availableChanged();
    void systemConnectionChanged(const QString &address, const QString &path,
                                 const QString &modelName, bool connected);

private slots:
    void onPropertiesChanged(const QDBusMessage &message);
    void onInterfacesChanged();

private:
    struct Device {
        QString path;
        QString name;
        QString modelName;
        QString address;
        bool connected;
        bool nothing;
    };

    static bool looksLikeNothing(const QString &name, const QStringList &uuids);
    static bool looksLikeAudio(const QString &icon, uint deviceClass, const QStringList &uuids);

    QList<Device> m_devices;
    QHash<QString, bool> m_knownConnected; // address -> last connected state
    bool m_available;
};

#endif // DEVICEMODEL_H
