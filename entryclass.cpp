#include "entryclass.h"
#include "gattserver.h"
#include "gattpacket.h"
#include "watchdatawriter.h"
#include "connectivity.h"
#include "packetreader.h"

#include "device/devicelocator.h"
#include "device/devicehandler.h"
#include "device/deviceservice.h"
#include "device/devicecharacteristic.h"

EntryClass::EntryClass(PacketReader *reader):
  m_packetReader(reader)
{
    connect(m_packetReader, &PacketReader::readyRead, this, &EntryClass::dataReceived, Qt::QueuedConnection);
    m_locator = new DeviceLocator();
    m_locator->setFilter("Pebble");
    connect(m_locator, &DeviceLocator::deviceAdded, this, &EntryClass::addDevice);
    connect(m_locator, &DeviceLocator::deviceRemoved, this, &EntryClass::removeDevice);
    m_locator->startDiscovery();
}

void EntryClass::addDevice(DeviceHandler *device)
{
    m_locator->stopDiscovery();

    qDebug() << "got device" << device->address() << device->name();

    m_handler = device;
    connect(m_handler, &DeviceHandler::connectedChanged, [this]() {
        if (m_handler->connected()) {
            qDebug() << "Connected to watch";
        }
    });

    m_handler->getService(pairingService, this, "pairToPebble");

    m_handler->connectToDevice();
}

void EntryClass::removeDevice(const QString &path)
{
    if (path == m_handler->path())
        m_handler = nullptr;
}

void EntryClass::pairToPebble(DeviceService* service)
{
    DeviceCharacteristic* characteristic = service->characteristic(pairingCharacteristic);
    m_connectivity = new Connectivity(service->characteristic(connectivityCharacteristic)->readCharacteristic());
    service->characteristic(connectivityCharacteristic)->subscribeToCharacteristic(m_connectivity, &Connectivity::connectivityFlagsChanged);

    if (!m_handler->paired() || !m_connectivity->paired()) {
        if (m_handler->paired() && !m_connectivity->paired()) {
            qDebug() << "FUCK WE ARENT PAIRED???";
            m_locator->removeDevice(m_handler->path());
            return;
        } else {
            QByteArray data = pairTriggers(true, false, true);

            // Trigger pair on watch
            characteristic->writeCharacteristic(data);

            // Trigger pair from our side
            m_handler->pair();
        }
    }
}

void EntryClass::dataReceived()
{
    uchar header[4];
    m_packetReader->peek(reinterpret_cast<char*>(header), 4);
    qDebug() << "Header is" << QByteArray::fromRawData(reinterpret_cast<char*>(header), 4).toHex();

    int packetLength = qFromBigEndian<quint16>(&header[0]);
    if (m_packetReader->bytesAvailable() < packetLength + 4) {
        qDebug() << "oh shit we don't have enough bytes";
        return;
    }

    QByteArray data = m_packetReader->read(m_packetReader->bytesAvailable());
    WatchDataReader reader(data);
    int length = reader.read<quint16>();
    int endpoint = reader.read<quint16>();
    qDebug() << "endpoint is" << endpoint << "and packet length is" << length;
    if (endpoint == 17) {
        QByteArray toSend;
        WatchDataWriter writer(&toSend);
        writer.writeLE<quint8>(0x01);        // ok
        writer.writeLE<quint32>(0xFFFFFFFF); // deprecated since 3.0
        writer.writeLE<quint32>(0);          // deprecated since 3.0
        writer.write<quint32>(2);
        writer.write<quint8>(2); // response version
        writer.write<quint8>(4); // major version
        writer.write<quint8>(4); // minor version
        writer.write<quint8>(3); // bugfix version
        writer.writeLE<quint64>(27055);


        QByteArray finalData;
        finalData.append((toSend.length() & 0xFF00) >> 8);
        finalData.append(toSend.length() & 0xFF);
        finalData.append((17 & 0xFF00) >> 8);
        finalData.append(17 & 0xFF);
        finalData.append(toSend);

        emit writeToPebble(finalData);
    } else if (endpoint == 49) {
        reader.read<quint8>();
        quint8 transaction = reader.read<quint8>();
        QByteArray ba(2, Qt::Uninitialized);
        ba[0] = 255;
        ba[1] = transaction;

        QByteArray finalData;
        finalData.append((ba.length() & 0xFF00) >> 8);
        finalData.append(ba.length() & 0xFF);
        finalData.append((49 & 0xFF00) >> 8);
        finalData.append(49 & 0xFF);
        finalData.append(ba);

        emit writeToPebble(finalData);
    } else if (endpoint == 2001) {
        QByteArray toSend;
        WatchDataWriter writer(&toSend);
        writer.write<quint8>(0);
        writer.write<quint32>(rand() % 0xFFFFFFFF);

        QByteArray finalData;
        finalData.append((toSend.length() & 0xFF00) >> 8);
        finalData.append(toSend.length() & 0xFF);
        finalData.append((2001 & 0xFF00) >> 8);
        finalData.append(2001 & 0xFF);
        finalData.append(toSend);

        emit writeToPebble(finalData);
    } else if (endpoint == 6778) {
        reader.read<quint8>();
        QByteArray message;
        WatchDataWriter writer(&message);
        writer.write<quint8>(133);
        writer.write<quint8>(reader.read<quint8>());


        QByteArray finalData;
        finalData.append((message.length() & 0xFF00) >> 8);
        finalData.append(message.length() & 0xFF);
        finalData.append((6778 & 0xFF00) >> 8);
        finalData.append(6778 & 0xFF);
        finalData.append(message);

        emit writeToPebble(finalData);
    }

    if (m_packetReader->bytesAvailable() > 0)
        dataReceived();
}

void EntryClass::connectedToPebble()
{
    qDebug() << "omaga we connected????";
}

QByteArray EntryClass::pairTriggers(bool supportsPinningWithoutSlaveSecurity, bool belowLollipop, bool clientMode)
{
    std::array<bool, 6> boolArray = { true, supportsPinningWithoutSlaveSecurity, false, belowLollipop, clientMode, false };
    QByteArray ret = boolArrayToBytes(boolArray);
    return ret;
}