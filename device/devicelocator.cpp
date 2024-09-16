#include "devicelocator.h"
#include "dbuswatcher.h"
#include "devicehandler.h"
#include <QLoggingCategory>
#include <QDBusMetaType>
#include <future>

DeviceLocator::DeviceLocator(QObject* parent):
  QObject(parent),
  watcher(new DBusWatcher(this))
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
        discoveryFilter.insert("Pattern", m_filter);
        m_discoveryIface->call("SetDiscoveryFilter", discoveryFilter);
    }
	qDebug() << "Started discovery!";
    connect(watcher, &DBusWatcher::deviceAdded, this, &DeviceLocator::deviceAdded);
    m_discoveryIface->call("StartDiscovery");
    watcher->setFilter("Pebble");

    QMetaObject::invokeMethod(watcher, &DBusWatcher::parsePairedDevices, Qt::DirectConnection);
}

void DeviceLocator::stopDiscovery()
{
    disconnect(watcher, &DBusWatcher::deviceAdded, this, &DeviceLocator::deviceAdded);
    m_discoveryIface->call("StopDiscovery");
}

void DeviceLocator::handleBusSocketActivated()
{
    watcher->handleBusSocketActivated();
}