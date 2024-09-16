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

class DeviceLocator : public QObject
{
    Q_OBJECT

public:
    DeviceLocator(QObject *parent = nullptr);
    void setFilter(QString filter);

    void startDiscovery();
    void stopDiscovery();

    inline QObject *getSender() {
        return this->sender();
    }

signals:
    void deviceAdded(DeviceHandler *device);

public slots:
    void handleBusSocketActivated();

// Member variables
private:
    QDBusInterface *m_iface, *m_discoveryIface;
    QString m_device; // Bluetooth HCI device, like /org/bluez/hci0
    QString m_filter;
    DBusWatcher* const watcher;
};

#endif // DEVICELOCATOR_H