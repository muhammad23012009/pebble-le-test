#ifndef DEVICELOCATOR_H
#define DEVICELOCATOR_H

#include <QObject>
#include <QDBusInterface>
#include <QDBusReply>
#include <dbus/dbus.h>
#include <QSocketNotifier>

typedef QMap<QString, QVariantMap> InterfaceList;
Q_DECLARE_METATYPE(InterfaceList)

class DBusWatcher;
class DeviceHandler;
class DeviceService;
class DeviceCharacteristic;
class DeviceDescriptor;

class DeviceLocator : public QObject
{
    Q_OBJECT

public:
    DeviceLocator(QObject *parent = nullptr);
    void setFilter(QString filter);

    void startDiscovery();
    void stopDiscovery();

    void removeDevice(const QString &path);

signals:
    void deviceAdded(DeviceHandler *device);
    void deviceRemoved(const QString &path);

public slots:
    void handleInterfacesAdded(const QDBusObjectPath &path, InterfaceList list);
    void handleInterfacesRemoved(const QDBusObjectPath &path, QStringList list);

// Member variables
private:
    void parsePairedDevices();

    QDBusInterface *m_iface, *m_discoveryIface;
    QString m_device; // Bluetooth HCI device, like /org/bluez/hci0
    QString m_filter;

	QHash<QString, DeviceHandler*> m_devices;
	QHash<QString, DeviceService*> m_services;
    QHash<QString, DeviceCharacteristic*> m_characteristics;
};

#endif // DEVICELOCATOR_H