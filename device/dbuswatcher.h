#ifndef DBUSWATCHER_H
#define DBUSWATCHER_H

#include <QObject>
#include <QDebug>
#include <QSocketNotifier>
#include <QMap>
#include <dbus/dbus.h>

class DeviceLocator;
class DeviceHandler;
class DeviceService;

class DBusWatcher : public QObject {
	Q_OBJECT
	public: DBusWatcher(DeviceLocator *parent);
    public: void setFilter(QString filter);

	void sendMessageWithString(const char *service, const char *path, const char *iface, const char *method, const char *arg);
	void addMatchRule(const char *rule);
	void removeMatchRule(const char *rule);

	static dbus_bool_t busWatchAdd(DBusWatch *watch, void *data);
	static void busWatchRemove(DBusWatch *watch, void *data);
	static void busWatchToggle(DBusWatch *watch, void *data);
	static DBusHandlerResult busMessageFilter(DBusConnection *conn, DBusMessage *msg, void *user_data);

	void parseCall(DBusMessage *msg);
    void parsePairedDevices();

	void handleBusSocketActivated();

	Q_SIGNAL void deviceAdded(DeviceHandler *device);
	Q_SIGNAL void serviceDiscovered(QString path, QVariantMap properties);

	QHash<QString, DeviceHandler*> m_devices;
	QHash<QString, DeviceService*> m_services;

    QString m_filter;

	DBusConnection *m_dbusConn;
};

#endif // DBUSWATCHER_H