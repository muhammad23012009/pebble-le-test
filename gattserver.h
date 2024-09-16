#include <QObject>
#include <QThread>
#include <QTimer>

#include <QLowEnergyAdvertisingData>
#include <QLowEnergyAdvertisingParameters>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyCharacteristicData>
#include <QLowEnergyDescriptorData>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyServiceData>

#include "gattpacket.h"

const QBluetoothUuid serviceUuid = QBluetoothUuid("10000000-328E-0FBB-C642-1AA6699BDADA");
const QBluetoothUuid writeCharacteristic = QBluetoothUuid("10000001-328E-0FBB-C642-1AA6699BDADA");
const QBluetoothUuid readCharacteristic = QBluetoothUuid("10000002-328E-0FBB-C642-1AA6699BDADA");

const QBluetoothUuid fakeServiceUuid = QBluetoothUuid("BADBADBA-DBAD-BADB-ADBA-BADBADBADBAD");

const int default_maxRxWindow = 25;
const int default_maxTxWindow = 25;

class GATTServer : public QObject
{
    Q_OBJECT

public:
    GATTServer();
    void sendAck(int sequence);
    void sendDataToPebble();

public slots:
    void run();
    void writeToPebble(QByteArray value);
    void processData(QByteArray data);

signals:
    void dataReceived(QByteArray data);

private:
    int m_sequence = 0;
    int m_remoteSequence = 0;
    inline int getNextSequence(int sequence) {
        return (sequence + 1) % 32;
    }

    int m_maxRxWindow = default_maxRxWindow;
    int m_maxTxWindow = default_maxTxWindow;

    QList<QByteArray> m_pendingPackets;
    QHash<int, GATTPacket> m_pendingAcks;

    GATTPacket m_lastAck;
    PPoGATTVersion m_gattVersion;

    int m_rxPending = 0;

    QLowEnergyController *m_leController;
    QLowEnergyService *m_service, *m_fakeService;
};