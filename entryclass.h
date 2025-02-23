#ifndef ENTRYCLASS_H
#define ENTRYCLASS_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QIODevice>
#include <QThreadPool>

#include "watchdatareader.h"

const QString pairingService = "0000fed9-0000-1000-8000-00805f9b34fb";
const QString pairingCharacteristic = "00000002-328e-0fbb-c642-1aa6699bdada";
const QString connectionParamsCharacteristic = "00000005-328E-0FBB-C642-1AA6699BDADA";
const QString connectivityCharacteristic = "00000001-328e-0fbb-c642-1aa6699bdada";

// PPoGATT
const QBluetoothUuid PPoGATT_service = QBluetoothUuid("30000003-328E-0FBB-C642-1AA6699BDADA");
const QBluetoothUuid PPoGATT_READ = QBluetoothUuid("30000004-328E-0FBB-C642-1AA6699BDADA");
const QBluetoothUuid PPoGATT_WRITE = QBluetoothUuid("30000006-328e-0fbb-c642-1aa6699bdada");
const QBluetoothUuid mtuCharacteristic = QBluetoothUuid("00000003-328e-0fbb-c642-1aa6699bdada");

class DeviceLocator;
class DeviceHandler;
class DeviceService;
class Connectivity;
class GATTServer;
class PacketReader;

class EntryClass : public QObject
{
    Q_OBJECT

public:
    EntryClass(PacketReader *reader);

public slots:
    void addDevice(DeviceHandler *device);
    void removeDevice(const QString &path);

    void pairToPebble(DeviceService* service);
    void connectedToPebble();
    void dataReceived();

signals:
    void writeToPebble(const QByteArray &data);

private:
    DeviceLocator *m_locator;

    DeviceHandler *m_handler;
    GATTServer *m_server;
    PacketReader *m_packetReader;

private:
    Connectivity *m_connectivity;

    QByteArray pairTriggers(bool supportsPinningWithoutSlaveSecurity, bool belowLollipop, bool clientMode);

    template <std::size_t N>
    QByteArray boolArrayToBytes(std::array<bool, N> &arr) {
        QByteArray ret;
        ret.reserve((arr.size() + 7) / 8);
        ret.resize((arr.size() + 7) / 8);
        for (int i = 0; i < arr.size(); i++) {
            int i2 = i / 8;
            int i3 = i % 8;
            if (arr.at(i)) {
                ret[i2] = (quint8) ((1 << i3) | ret[i2]);
            }
        }
        return ret;
    }
};

#endif // ENTRYCLASS_H