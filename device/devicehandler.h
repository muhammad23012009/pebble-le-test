#ifndef DEVICEHANDLER_H
#define DEVICEHANDLER_H

#include <QObject>
#include <QDBusInterface>
#include <QTimer>
#include <QPointer>
#include <QMetaMethod>

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

    QString path() const;
    QString name() const;
    QString address() const;

    bool connected() const;
    bool paired() const;
    bool servicesDiscovered() const;

    bool hasService(QString serviceUuid) const;
    bool hasServices() const;

    void getService(const QString& serviceUuid, QObject *obj, const QString& method);

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
    QString m_path;
    QVariantMap m_info;
    QTimer *m_reconnectTimer;

    // Each service is associated with a UUID
    QHash<QString, DeviceService*> m_services;

    class Callback {
    public:
        QPointer<QObject> obj;
        QString method;
    };
    QMap<QString, Callback> m_callbacks;

    bool m_connected = false;
    bool m_paired = false;
    bool m_servicesDiscovered = false;
};

#endif // DEVICEHANDLER_H