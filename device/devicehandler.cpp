#include "devicehandler.h"
#include "deviceservice.h"

DeviceHandler::DeviceHandler(QString path, QVariantMap &info, QObject *parent):
  m_path(path),
  m_info(info),
  QObject(parent)
{
    m_iface = new QDBusInterface("org.bluez", path, "org.bluez.Device1", QDBusConnection::systemBus(), this);
    m_reconnectTimer = new QTimer(this);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DeviceHandler::connectToDevice);

    m_connected = m_iface->property("Connected").toBool();
    m_paired = m_iface->property("Paired").toBool();
    m_servicesDiscovered = m_iface->property("ServicesResolved").toBool();

    QDBusConnection::systemBus().connect("org.bluez", path, "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(propertiesChanged(QString, QVariantMap, QStringList)));

    qDebug() << "meow" << m_iface->property("ManufacturerData");
}

DeviceHandler::~DeviceHandler()
{
    m_services.clear();
}

void DeviceHandler::connectToDevice()
{
    if (!m_connected)
        m_iface->asyncCall("Connect");
    else {
        emit connectedChanged();

        if (m_services.size() > 0)
            emit servicesChanged();

        if (m_reconnectTimer->isActive())
            m_reconnectTimer->stop();
    }
}

void DeviceHandler::disconnectFromDevice()
{
    m_iface->asyncCall("Disconnect");
}

void DeviceHandler::pair() const
{
    m_iface->call("Pair");
}

QString DeviceHandler::path() const
{
    qDebug() << "path is" << m_path;
    return m_path;
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

void DeviceHandler::getService(const QString& serviceUuid, QObject* obj, const QString& method)
{
    if (m_services.contains(serviceUuid)) {
        QMetaObject::invokeMethod(obj, method.toLatin1(), Q_ARG(DeviceService*, m_services.value(serviceUuid)), Qt::QueuedConnection);
    } else {
        Callback cb;
        cb.obj = obj;
        cb.method = method;

        m_callbacks.insert(serviceUuid, cb);
    }
}

void DeviceHandler::propertiesChanged(QString interface, QVariantMap properties, QStringList /*invalid_properties*/)
{
    if (interface == "org.bluez.Device1") {
        if (properties.contains("Connected")) {
            m_connected = properties.value("Connected").toBool();
            emit connectedChanged();

            if (!m_connected) {
                // Device disconnected, start the reconnection timer
                qDebug() << "Starting reconnection timer";
                m_reconnectTimer->start(5 * 1000);
            } else {
                if (m_reconnectTimer->isActive()) {
                    qDebug() << "Stopping reconnection timer";
                    m_reconnectTimer->stop();
                }
            }

            if (properties.contains("ServicesResolved") || m_servicesDiscovered) {
                emit servicesChanged();
            }
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

    if (m_callbacks.contains(service->serviceUuid())) {
        Callback cb = m_callbacks.take(service->serviceUuid());
        QMetaObject::invokeMethod(cb.obj.data(), cb.method.toLatin1(), Q_ARG(DeviceService*, service));
    }
}