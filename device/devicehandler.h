#ifndef DEVICEHANDLER_H
#define DEVICEHANDLER_H

#include <QObject>
#include <QDBusInterface>
#include <QPointer>

class DeviceService;

class DeviceHandler : public QObject
{
    Q_OBJECT
public:
    DeviceHandler(QString path, QVariantMap &info, QObject *parent = nullptr);
    ~DeviceHandler();

    void connectToDevice();
    void disconnectFromDevice();
    void pair() const;

    QString name() const;
    QString address() const;

    bool connected() const;
    bool paired() const;
    bool servicesDiscovered() const;

    bool hasService(QString serviceUuid) const;
    bool hasServices() const;

    DeviceService *getService(QString serviceUuid);

    // Called by dbus watcher
    void serviceDiscovered(DeviceService *service);

signals:
    void connectedChanged();
    void pairedChanged();
    void servicesChanged();

private slots:
    void propertiesChanged(QString interface, QVariantMap properties, QStringList /*invalid_properties*/);

private:
    QDBusInterface *m_iface;
    QVariantMap m_info;

    // Each service is associated with a UUID
    QHash<QString, DeviceService*> m_services;

    bool m_connected = false;
    bool m_paired = false;
    bool m_servicesDiscovered = false;
};

#endif // DEVICEHANDLER_H