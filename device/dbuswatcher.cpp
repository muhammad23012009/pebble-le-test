#include "dbuswatcher.h"
#include "devicelocator.h"
#include "devicehandler.h"
#include "deviceservice.h"

DBusWatcher::DBusWatcher(DeviceLocator *parent):
  QObject(parent)
{
	m_dbusConn = dbus_bus_get(DBUS_BUS_SYSTEM, nullptr);
    dbus_connection_set_exit_on_disconnect(m_dbusConn, FALSE);
	dbus_connection_set_watch_functions(m_dbusConn, busWatchAdd, busWatchRemove,
										busWatchToggle, this, NULL);

	addMatchRule("type='signal',sender='org.bluez',interface='org.freedesktop.DBus.ObjectManager',path='/',member='InterfacesAdded'");

	dbus_bool_t result = dbus_connection_add_filter(m_dbusConn, busMessageFilter,
													this, NULL);	
}

void DBusWatcher::sendMessageWithString(const char *service, const char *path, const char *iface, const char *method, const char *arg)
{
	DBusMessage *msg = dbus_message_new_method_call(service, path, iface, method);
	Q_ASSERT(msg);
	dbus_message_set_no_reply(msg, TRUE);
	dbus_message_append_args(msg,
							 DBUS_TYPE_STRING, &arg,
							 DBUS_TYPE_INVALID);
	dbus_connection_send(m_dbusConn, msg, NULL);
	dbus_message_unref(msg);
}

void DBusWatcher::addMatchRule(const char *rule)
{
	sendMessageWithString("org.freedesktop.DBus", "/",
						  "org.freedesktop.DBus", "AddMatch", rule);
}

void DBusWatcher::removeMatchRule(const char *rule)
{
	sendMessageWithString("org.freedesktop.DBus", "/",
						  "org.freedesktop.DBus", "RemoveMatch", rule);
}

dbus_bool_t DBusWatcher::busWatchAdd(DBusWatch *watch, void *data)
{
	DBusWatcher *self = static_cast<DBusWatcher*>(data);
	DeviceLocator *monitor = static_cast<DeviceLocator*>(self->parent());
	int socket = dbus_watch_get_socket(watch);
	int flags = dbus_watch_get_flags(watch);

	QSocketNotifier::Type type;
	switch (flags) {
	case DBUS_WATCH_READABLE:
		type = QSocketNotifier::Read;
		break;
	case DBUS_WATCH_WRITABLE:
		type = QSocketNotifier::Write;
		break;
	default:
		qWarning() << "Can't add this type of watch" << flags;
		return FALSE;
	}

	QSocketNotifier *notifier = new QSocketNotifier(socket, type, monitor);
	dbus_watch_set_data(watch, notifier, NULL);

	notifier->setEnabled(dbus_watch_get_enabled(watch));
	notifier->setProperty("dbus-watch", QVariant::fromValue<void*>(watch));

	notifier->connect(notifier, &QSocketNotifier::activated,
					  monitor, &DeviceLocator::handleBusSocketActivated);

	return TRUE;
}

void DBusWatcher::busWatchRemove(DBusWatch *watch, void *data)
{
	QSocketNotifier *notifier = static_cast<QSocketNotifier*>(dbus_watch_get_data(watch));
	Q_UNUSED(data);
	delete notifier;
}

void DBusWatcher::busWatchToggle(DBusWatch *watch, void *data)
{
	QSocketNotifier *notifier = static_cast<QSocketNotifier*>(dbus_watch_get_data(watch));
	Q_UNUSED(data);
	notifier->setEnabled(dbus_watch_get_enabled(watch));
}

DBusHandlerResult DBusWatcher::busMessageFilter(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	DBusWatcher *self = static_cast<DBusWatcher*>(user_data);
	DBusError error = DBUS_ERROR_INIT;
	Q_UNUSED(conn);
	switch (dbus_message_get_type(msg)) {
	case DBUS_MESSAGE_TYPE_SIGNAL:
		if (dbus_message_is_signal(msg, "org.freedesktop.DBus.ObjectManager", "InterfacesAdded")) {
			self->parseCall(msg);
		}
		break;
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

void DBusWatcher::handleBusSocketActivated()
{
	DeviceLocator *loc = static_cast<DeviceLocator*>(parent());
	QSocketNotifier *notifier = static_cast<QSocketNotifier*>(loc->getSender());
	DBusWatch *watch = static_cast<DBusWatch*>(notifier->property("dbus-watch").value<void*>());

	dbus_watch_handle(watch, dbus_watch_get_flags(watch));

	while (dbus_connection_get_dispatch_status(m_dbusConn) == DBUS_DISPATCH_DATA_REMAINS) {
		dbus_connection_dispatch(m_dbusConn);
	}
}

void DBusWatcher::parseCall(DBusMessage *msg)
{
	DBusMessageIter iter, sub;
	QVariantMap props;
	const char *path, *keyName;

	dbus_message_iter_init(msg, &iter);
	dbus_message_iter_get_basic(&iter, &path);
	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &sub);
	while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter entry, value;
		dbus_message_iter_recurse(&sub, &entry);
		const char* previousKey = keyName;
		dbus_message_iter_get_basic(&entry, &keyName);

		if (std::string(keyName).find("org.bluez") == std::string::npos) {
			dbus_message_iter_next(&sub);
			keyName = previousKey;
			continue;
		}

		dbus_message_iter_next(&entry);

		dbus_message_iter_recurse(&entry, &value);
		while (dbus_message_iter_get_arg_type(&value) == DBUS_TYPE_DICT_ENTRY) {
			const char* key;
			DBusMessageIter variantEntry, variantValue;
			dbus_message_iter_recurse(&value, &variantEntry);
			dbus_message_iter_get_basic(&variantEntry, &key);
			dbus_message_iter_next(&variantEntry);
			dbus_message_iter_recurse(&variantEntry, &variantValue);

			int arg_type = dbus_message_iter_get_arg_type(&variantValue);

			if (arg_type == DBUS_TYPE_UINT32) {
				uint32_t value;
				dbus_message_iter_get_basic(&variantValue, &value);
				props.insert(key, value);
			} else if (arg_type == DBUS_TYPE_STRING || arg_type == DBUS_TYPE_OBJECT_PATH) {
				const char* value;
				dbus_message_iter_get_basic(&variantValue, &value);
				props.insert(key, value);
			} else if (arg_type == DBUS_TYPE_BOOLEAN) {
				bool boolean;
				dbus_message_iter_get_basic(&variantValue, &boolean);
				props.insert(key, boolean);
			} else if (arg_type == DBUS_TYPE_ARRAY) {
				// Most likely UUID list
				DBusMessageIter subIter;
				QStringList list;
				dbus_message_iter_recurse(&variantValue, &subIter);
				while (dbus_message_iter_get_arg_type(&subIter) == DBUS_TYPE_STRING) {
					const char* val;
					dbus_message_iter_get_basic(&subIter, &val);
					list.append(val);
					dbus_message_iter_next(&subIter);
				}
				props.insert(key, list);
			}

			dbus_message_iter_next(&value);
		}

		dbus_message_iter_next(&sub);
	}

	if (std::string(keyName) == "org.bluez.Device1") {
        if (m_filter.isEmpty() || props.value("Name").toString().contains(m_filter)) {
		    DeviceHandler* device = new DeviceHandler(path, props);
		    m_devices.insert(path, device);
		    emit deviceAdded(device);
        }

	} else if (std::string(keyName) == "org.bluez.GattService1") {
		if (m_devices.contains(props.value("Device").toString())) {
			DeviceService *service = new DeviceService(path, props);
			m_devices.value(props.value("Device").toString())->serviceDiscovered(service);
			m_services.insert(path, service);
		}

	} else if (std::string(keyName) == "org.bluez.GattCharacteristic1") {
		if (m_services.contains(props.value("Service").toString())) {
			m_services.value(props.value("Service").toString())->addCharacteristic(path, props);
		}
	}
}

// Asynchronously called with QMetaObject::invokeMethod()
void DBusWatcher::parsePairedDevices()
{
    DBusMessage *msg;
    DBusMessageIter iter, sub;
    DBusMessage *ret;

    msg = dbus_message_new_method_call("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    ret = dbus_connection_send_with_reply_and_block(m_dbusConn, msg, -1, nullptr);

    dbus_message_iter_init(ret, &iter);
    dbus_message_iter_recurse(&iter, &sub);
    while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry, value;
        const char *path;
        QVariantMap props;
        dbus_message_iter_recurse(&sub, &entry);
        dbus_message_iter_get_basic(&entry, &path);

        dbus_message_iter_next(&entry);
        while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_ARRAY) {
            DBusMessageIter arrayIter;
            dbus_message_iter_recurse(&entry, &arrayIter);

            while (dbus_message_iter_get_arg_type(&arrayIter) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter entry;
                const char *keyName;
                dbus_message_iter_recurse(&arrayIter, &entry);

                if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
                    dbus_message_iter_get_basic(&entry, &keyName);
                    dbus_message_iter_next(&entry);
                }

                if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_ARRAY) {
                    DBusMessageIter arrayIter;
                    dbus_message_iter_recurse(&entry, &arrayIter);
                    while (dbus_message_iter_get_arg_type(&arrayIter) == DBUS_TYPE_DICT_ENTRY) {
                        DBusMessageIter entry;
                        const char* key;
                        dbus_message_iter_recurse(&arrayIter, &entry);
                        dbus_message_iter_get_basic(&entry, &key);
                        dbus_message_iter_next(&entry);

                        if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {
                            DBusMessageIter variantIter;
                            dbus_message_iter_recurse(&entry, &variantIter);

                            int arg_type = dbus_message_iter_get_arg_type(&variantIter);

                            if (arg_type == DBUS_TYPE_STRING || arg_type == DBUS_TYPE_OBJECT_PATH) {
                                const char* value;
                                dbus_message_iter_get_basic(&variantIter, &value);
                                props.insert(key, value);
                            } else if (arg_type == DBUS_TYPE_BOOLEAN) {
                                bool value;
                                dbus_message_iter_get_basic(&variantIter, &value);
                                props.insert(key, value);
                            }
                        }
                        dbus_message_iter_next(&arrayIter);
                    }
                }

                dbus_message_iter_next(&arrayIter);

                if (std::string(keyName) == "org.bluez.Device1") {
                    if (props.value("Paired").toBool()) {
                        if (m_filter.isEmpty() || props.value("Name").toString().contains(m_filter)) {
                            DeviceHandler *device = new DeviceHandler(path, props);
                            m_devices.insert(path, device);
                            emit deviceAdded(device);
                        }
                    }
                } else if (std::string(keyName) == "org.bluez.GattService1") {
                    qDebug() << "parsing a service" << m_devices.keys();
                    if (m_devices.contains(props.value("Device").toString())) {
                        DeviceService *service = new DeviceService(path, props);
			            m_devices.value(props.value("Device").toString())->serviceDiscovered(service);
		    	        m_services.insert(path, service);
                    }
                } else if (std::string(keyName) == "org.bluez.GattCharacteristic1") {
	                if (m_services.contains(props.value("Service").toString())) {
		                m_services.value(props.value("Service").toString())->addCharacteristic(path, props);
	    	        }
                }
            }

            dbus_message_iter_next(&entry);
        }
        dbus_message_iter_next(&sub);
    }
    // This is a bit crude, but my brain could only think of this as a viable solution at 2am...
    foreach (DeviceHandler *device, m_devices.values()) {
        if (device->hasServices())
            emit device->servicesChanged();
    }
}

void DBusWatcher::setFilter(QString filter)
{
    m_filter = filter;
}