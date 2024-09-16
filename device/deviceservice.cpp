#include "deviceservice.h"

DeviceService::DeviceService(QString path, QVariantMap &properties):
  m_info(properties)
{
}

QString DeviceService::serviceUuid() const
{
    return m_info.value("UUID").toString();
}

QString DeviceService::devicePath() const
{
    return m_info.value("Device").toString();
}

void DeviceService::readCharacteristic(QString uuid)
{
    GET_IFACE(m_characteristics.value(uuid.toLower()));
    QVariantMap options;
    iface.call("ReadValue", options);
}

void DeviceService::writeCharacteristic(QString uuid, QByteArray &data)
{
    GET_IFACE(m_characteristics.value(uuid.toLower()));
    QVariantMap options;
    iface.call("WriteValue", data, options);
}

void DeviceService::startNotify(QString uuid)
{
    GET_IFACE(m_characteristics.value(uuid.toLower()));
    iface.call("StartNotify");
}

void DeviceService::addCharacteristic(QString path, QVariantMap &properties)
{
    m_characteristics.insert(properties.value("UUID").toString(), path);
    emit characteristicAdded(properties.value("UUID").toString());
}