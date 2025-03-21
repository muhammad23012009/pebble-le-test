#include "devicedescriptor.h"

DeviceDescriptor::DeviceDescriptor(const QString &path, const QVariantMap &properties, QObject *parent):
  m_path(path),
  m_properties(properties),
  QObject(parent)
{
    m_iface = new QDBusInterface("org.bluez", path, "org.bluez.GattDescriptor1", QDBusConnection::systemBus(), this);
}

QByteArray DeviceDescriptor::readDescriptor()
{
    QDBusReply<QByteArray> reply = m_iface->call("ReadValue");
    if (reply.isValid())
        return reply.value();
    
    return QByteArray();
}

void DeviceDescriptor::writeDescriptor(const QByteArray &value)
{
    m_iface->call("WriteValue", value);
}