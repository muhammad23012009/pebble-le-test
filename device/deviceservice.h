#ifndef DEVICESERVICE_H
#define DEVICESERVICE_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>

#define GET_IFACE(path) \
    QDBusInterface iface("org.bluez", path, "org.bluez.GattCharacteristic1", QDBusConnection::systemBus());

class DeviceService : public QObject
{
    Q_OBJECT

public:
    DeviceService(QString path, QVariantMap &properties);

    QString serviceUuid() const;

    QString devicePath() const;

    void readCharacteristic(QString uuid);
    void writeCharacteristic(QString uuid, QByteArray &data);

    void startNotify(QString uuid);

    // Called by dbus watcher
    void addCharacteristic(QString path, QVariantMap &properties);

signals:
    void characteristicAdded(QString uuid);

private:
    QVariantMap m_info;

    // Hash of UUID and path
    QHash<QString, QString> m_characteristics;

    Q_DISABLE_COPY(DeviceService)
};

#endif // DEVICESERVICE_H