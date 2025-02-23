#include "devicelocator.h"
#include "devicehandler.h"
#include "deviceservice.h"
#include <QLoggingCategory>
#include <QDBusMetaType>
#include <future>

DeviceLocator::DeviceLocator(QObject* parent):
  QObject(parent)
{
	qDBusRegisterMetaType<InterfaceList>();
	qDBusRegisterMetaType<QMap<QDBusObjectPath, InterfaceList>>();
    m_iface = new QDBusInterface("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", QDBusConnection::systemBus());

    QDBusReply<QMap<QDBusObjectPath, InterfaceList>> reply = m_iface->call("GetManagedObjects");

    for (const QDBusObjectPath &path: static_cast<const QList<QDBusObjectPath>>(reply.value().keys())) {
        const InterfaceList ifaceList = reply.value().value(path);
        for (const QString &iface: static_cast<const QList<QString>>(ifaceList.keys())) {
            if (iface == QStringLiteral("org.bluez.Adapter1")) {
				m_device = path.path();
                break;
            }
        }
		if (!m_device.isEmpty())
			break;
    }

    QDBusConnection::systemBus().connect("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "InterfacesAdded", this, SLOT(handleInterfacesAdded(const QDBusObjectPath&, InterfaceList)));
    QDBusConnection::systemBus().connect("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved", this, SLOT(handleInterfacesRemoved(const QDBusObjectPath&, QStringList)));

    m_discoveryIface = new QDBusInterface("org.bluez", m_device, "org.bluez.Adapter1", QDBusConnection::systemBus());
}

void DeviceLocator::setFilter(QString filter)
{
    m_filter = filter;
}

void DeviceLocator::startDiscovery()
{
    if (!m_filter.isEmpty()) {
        QVariantMap discoveryFilter;
        discoveryFilter.insert("Transport", "le");
        discoveryFilter.insert("Pattern", m_filter);
        m_discoveryIface->call("SetDiscoveryFilter", discoveryFilter);
    }
	qDebug() << "Started discovery!";
    m_discoveryIface->call("StartDiscovery");
    parsePairedDevices();
}

void DeviceLocator::stopDiscovery()
{
    qDebug() << "Stopping discovery!";
    m_discoveryIface->call("StopDiscovery");
}

void DeviceLocator::removeDevice(const QString &path)
{
    m_discoveryIface->call("RemoveDevice", QDBusObjectPath(path));
}

void DeviceLocator::handleInterfacesAdded(const QDBusObjectPath &path, InterfaceList list)
{
    QVariantMap properties;

    if (list.contains("org.bluez.Device1")) {
        properties = list.value("org.bluez.Device1");
        if (m_filter.isEmpty() || properties.value("Name").toString().contains(m_filter)) {
            DeviceHandler *device = new DeviceHandler(path.path(), properties);
            m_devices.insert(path.path(), device);
            emit deviceAdded(device);
        }
    }
    
    if (list.contains("org.bluez.GattService1")) {
        properties = list.value("org.bluez.GattService1");
        QString devicePath = qvariant_cast<QDBusObjectPath>(properties.value("Device")).path();
		if (m_devices.contains(devicePath)) {
			DeviceService *service = new DeviceService(path.path(), properties, m_devices.value(devicePath));
			m_devices.value(devicePath)->serviceDiscovered(service);
			m_services.insert(path.path(), service);
		}
    }

    if (list.contains("org.bluez.GattCharacteristic1")) {
        properties = list.value("org.bluez.GattCharacteristic1");
        QString servicePath = qvariant_cast<QDBusObjectPath>(properties.value("Service")).path();
		if (m_services.contains(servicePath)) {
			m_services.value(servicePath)->addCharacteristic(path.path(), properties);
		}
    }
}

void DeviceLocator::handleInterfacesRemoved(const QDBusObjectPath &path, QStringList list)
{
    if (list.contains("org.bluez.Device1") && m_devices.contains(path.path())) {
        emit deviceRemoved(path.path());

        DeviceHandler *device = m_devices.take(path.path());
        device->deleteLater();
    }
    if (m_devices.isEmpty())
        startDiscovery();
}

void DeviceLocator::parsePairedDevices()
{
    QDBusReply<QMap<QDBusObjectPath, InterfaceList>> reply = m_iface->call("GetManagedObjects");

    for (auto it = reply.value().constBegin(), end = reply.value().constEnd(); it != end; ++it) {
        handleInterfacesAdded(it.key(), it.value());
    }
}