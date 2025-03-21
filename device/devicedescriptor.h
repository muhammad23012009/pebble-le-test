#ifndef DEVICEDESCRIPTOR_H
#define DEVICEDESCRIPTOR_H

#include <QObject>
#include <QVariantMap>
#include <QDBusInterface>
#include <QDBusReply>

class DeviceDescriptor : public QObject
{
    Q_OBJECT
public:
    DeviceDescriptor(const QString &path, const QVariantMap &properties, QObject *parent = nullptr);

    QByteArray readDescriptor();
    void writeDescriptor(const QByteArray &value);

private:
    QString m_path;
    QVariantMap m_properties;
    QDBusInterface *m_iface;
};

#endif