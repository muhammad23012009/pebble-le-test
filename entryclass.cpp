#include "entryclass.h"
#include "gattserver.h"
#include "gattpacket.h"
#include "watchdatawriter.h"

#include "device/devicelocator.h"
#include "device/devicehandler.h"
#include "device/deviceservice.h"

EntryClass::EntryClass()
{
    m_locator = new DeviceLocator();
    m_locator->setFilter("Pebble");
    connect(m_locator, &DeviceLocator::deviceAdded, this, &EntryClass::addDevice);
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

            if (m_handler->paired()) {
                qDebug() << "OH WOW WE PAIED????";
            }
        }
    });
    m_handler->connectToDevice();

    connect(m_handler, &DeviceHandler::servicesChanged, [this]() {
        if (!m_handler->paired()) {
            DeviceService *service = m_handler->getService(pairingService);
            QByteArray data = pairTriggers(true, false, true);

            // Trigger pair on watch
            service->writeCharacteristic(pairingCharacteristic, data);

            // Trigger pair from our side
            m_handler->pair();
        }
    });
}

void EntryClass::dataReceived(QByteArray data)
{
    WatchDataReader reader(data);
    int length = reader.read<quint16>();
    int endpoint = reader.read<quint16>();
    qDebug() << "endpoint is" << endpoint;
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
    }
}

QByteArray EntryClass::pairTriggers(bool supportsPinningWithoutSlaveSecurity, bool belowLollipop, bool clientMode)
{
    std::array<bool, 6> boolArray = { true, supportsPinningWithoutSlaveSecurity, false, belowLollipop, clientMode, false };
    QByteArray ret = boolArrayToBytes(boolArray);
    return ret;
}