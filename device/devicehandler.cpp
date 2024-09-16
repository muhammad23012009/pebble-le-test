#include "devicehandler.h"
#include "deviceservice.h"

DeviceHandler::DeviceHandler(QString path, QVariantMap &info, QObject *parent):
  m_info(info),
  QObject(parent)
{
    m_iface = new QDBusInterface("org.bluez", path, "org.bluez.Device1", QDBusConnection::systemBus());

    m_connected = m_iface->property("Connected").toBool();
    m_paired = m_iface->property("Paired").toBool();
    m_servicesDiscovered = m_iface->property("ServicesResolved").toBool();

    qDebug() << m_iface->property("ServiceData");

    QDBusConnection::systemBus().connect("org.bluez", path, "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(propertiesChanged(QString, QVariantMap, QStringList)));
}

DeviceHandler::~DeviceHandler()
{
    delete m_iface;
}

void DeviceHandler::connectToDevice()
{
    if (!m_connected)
        m_iface->asyncCall("Connect");
    else
        emit connectedChanged();
}

void DeviceHandler::disconnectFromDevice()
{
    m_iface->asyncCall("Disconnect");
}

void DeviceHandler::pair() const
{
    m_iface->call("Pair");
}

QString DeviceHandler::name() const
{
    return m_info.value("Name").toString();
}

QString DeviceHandler::address() const
{
    return m_info.value("Address").toString();
}

bool DeviceHandler::connected() const
{
    return m_connected;
}

bool DeviceHandler::paired() const
{
    return m_paired;
}

bool DeviceHandler::servicesDiscovered() const
{
    return m_servicesDiscovered;
}

bool DeviceHandler::hasService(QString serviceUuid) const
{
    return m_services.contains(serviceUuid);
}

bool DeviceHandler::hasServices() const
{
    return !m_services.isEmpty();
}

DeviceService *DeviceHandler::getService(QString serviceUuid)
{
    return m_services.value(serviceUuid);
}

void DeviceHandler::propertiesChanged(QString interface, QVariantMap properties, QStringList /*invalid_properties*/)
{
    if (interface == "org.bluez.Device1") {
        if (properties.contains("Connected")) {
            m_connected = properties.value("Connected").toBool();
            emit connectedChanged();
        }
        
        if (properties.contains("Paired")) {
            m_paired = properties.value("Paired").toBool();
            if (m_paired && !m_iface->property("Trusted").toBool()) {
                m_iface->setProperty("Trusted", true);
            }
            emit pairedChanged();
        }

        if (properties.contains("ServicesResolved")) {
            m_servicesDiscovered = properties.value("ServicesResolved").toBool();
            if (m_servicesDiscovered)
                emit servicesChanged();
        }
    }
}

void DeviceHandler::serviceDiscovered(DeviceService *service)
{
    m_services.insert(service->serviceUuid(), service);
}