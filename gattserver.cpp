#include "gattserver.h"
#include "watchdatareader.h"
#include "watchdatawriter.h"
#include "packetreader.h"
#include <QEventLoop>
#include <QByteArray>

#include <QList>
#include <QLoggingCategory>
#include <QTimer>
#include <memory>

GATTServer::GATTServer(PacketReader *reader):
  m_packetReader(reader)
{
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));

    m_leController = QLowEnergyController::createPeripheral(this);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, [=]() { this->sendAck(-1, true); });
}

void GATTServer::run()
{
    qDebug() << "GATT server started!";

    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(serviceUuid);

    // Fake service needed by Pebble for some reason
    QLowEnergyServiceData fakeServiceData;
    fakeServiceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    fakeServiceData.setUuid(fakeServiceUuid);
    QLowEnergyCharacteristicData fakeServiceChar;
    fakeServiceChar.setUuid(fakeServiceUuid);
    fakeServiceChar.setProperties(QLowEnergyCharacteristic::Read);
    fakeServiceChar.setValue(QByteArray(2, 0));
    fakeServiceData.addCharacteristic(fakeServiceChar);

    QLowEnergyCharacteristicData writeChar;
    writeChar.setUuid(writeCharacteristic);
    writeChar.setProperties(QLowEnergyCharacteristic::WriteNoResponse | QLowEnergyCharacteristic::Notify);
    const QLowEnergyDescriptorData clientConfig(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration,
                                                QByteArray(2, 0));
    writeChar.addDescriptor(clientConfig);
    serviceData.addCharacteristic(writeChar);

    QLowEnergyCharacteristicData readChar;
    readChar.setUuid(readCharacteristic);
    readChar.setValue(QByteArray::fromHex("00010000000000000000000000000000000001"));
    readChar.setProperties(QLowEnergyCharacteristic::Read);
    serviceData.addCharacteristic(readChar);

    m_service = m_leController->addService(serviceData);
    m_fakeService = m_leController->addService(fakeServiceData);

    connect(m_service, &QLowEnergyService::characteristicChanged, [this](const QLowEnergyCharacteristic &info, const QByteArray &value) {
        GATTPacket packet(value);

        if (packet.type() == GATTPacket::RESET_ACK) {
            m_maxRxWindow = packet.getMaxRxWindow();
            m_maxTxWindow = packet.getMaxTxWindow();

            QByteArray resetData;
            if (m_gattVersion.supportsWindowNegotiation) {
                resetData.append(25);
                resetData.append(25);
            }
            GATTPacket reset(GATTPacket::RESET_ACK, 0, resetData);
            m_service->writeCharacteristic(info, reset.data());
            qDebug() << "windows negotiated" << m_maxRxWindow << m_maxTxWindow;

            m_packetReader->open(QIODevice::ReadWrite);
            if (!m_connectionOpened) {
                m_connectionOpened = true;
                emit connectedToPebble();
            }

        } else if (packet.type() == GATTPacket::ACK) {
            // TODO: make this faster somehow, needs more optimization
            //qDebug() << "Received ACK from watch" << packet.sequence();
            for (int i = 0; i <= packet.sequence(); i++) {
                m_sentPackets = m_sentPackets > 0 ? (m_sentPackets - 1) : 0;
                m_pendingAcks.remove(i);
            }

            // Send all other packets
            if (m_pendingPackets.length() > 0)
                sendDataToPebble();

        } else if (packet.type() == GATTPacket::DATA) {
            qDebug() << "packet sequence is" << packet.sequence();
            qDebug() << "data is" << packet.data().toHex();
            if (m_remoteSequence == packet.sequence()) {
                QByteArray data = packet.data().remove(0, 1);

                sendAck(packet.sequence());
                m_remoteSequence = getNextSequence(m_remoteSequence);

                m_packetReader->write(data);
            } else {
                sendAck(m_lastAck.sequence());
            }

        } else if (packet.type() == GATTPacket::RESET) {
            // Clean up everything
            m_sequence = 0;
            m_remoteSequence = 0;
            m_pendingPackets.clear();
            m_pendingAcks.clear();
            m_sentPackets = 0;

            GATTPacket reset(GATTPacket::RESET, 0, QByteArray(1, packet.version().version));
            m_service->writeCharacteristic(info, reset.data());

            m_gattVersion = packet.version();
            QByteArray resetData;
            if (m_gattVersion.supportsWindowNegotiation) {
                /*resetData.append(m_maxRxWindow);
                resetData.append(m_maxTxWindow);*/
                resetData.append(25);
                resetData.append(25);
            }
            reset = GATTPacket(GATTPacket::RESET_ACK, 0, resetData);
            m_service->writeCharacteristic(info, reset.data());
        }
    });

    connect(m_service, &QLowEnergyService::descriptorWritten, [this](const QLowEnergyDescriptor &desc, const QByteArray &data) {
        qDebug() << "well shit guys" << desc.uuid() << data.toHex();
    });

    connect(m_leController, &QLowEnergyController::disconnected, [this, serviceData, fakeServiceData]() {
        m_service = m_leController->addService(serviceData);
        m_fakeService = m_leController->addService(fakeServiceData);

        m_leController->startAdvertising(QLowEnergyAdvertisingParameters(), QLowEnergyAdvertisingData());
    });

    QLowEnergyAdvertisingData data;
    data.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    data.setServices(QList<QBluetoothUuid>() << serviceUuid << fakeServiceUuid);
    QLowEnergyAdvertisingParameters params;
    params.setMode(QLowEnergyAdvertisingParameters::AdvInd);
    params.setInterval(1, 10);
    m_leController->startAdvertising(params, QLowEnergyAdvertisingData());
}

void GATTServer::sendAck(int sequence, bool forceAck)
{
    if (sequence == -1)
        sequence = m_ackSequence;
    else
        m_ackSequence = sequence;

    if (forceAck || m_timer->isActive())
        m_timer->stop();

    GATTPacket ack(GATTPacket::ACK, sequence, QByteArray());
    if (!m_gattVersion.supportsCoalescedAcking) {
        m_service->writeCharacteristic(m_service->characteristic(writeCharacteristic), ack.data());
        m_lastAck = ack;
    } else {
        m_rxPending++;
        if ((m_rxPending >= m_maxRxWindow / 2) || forceAck) {
            m_rxPending = 0;
            m_service->writeCharacteristic(m_service->characteristic(writeCharacteristic), ack.data());
            qDebug() << "Went ahead and wrote ACK with sequence" << sequence;
            m_lastAck = ack;
        } else {
            m_timer->start(200);
        }
    }
}

void GATTServer::writeToPebble(const QByteArray &data)
{
    qDebug() << "Writing data to pebble!" << m_sequence << m_pendingPackets.length();
    m_pendingPackets.append(data);
    m_pendingAcks.insert(m_sequence, GATTPacket(data));

    if (m_pendingPackets.length() <= 1 && m_pendingAcks.size() <= m_maxRxWindow)
        sendDataToPebble();
}

void GATTServer::sendDataToPebble()
{
    while (m_pendingPackets.size() != 0) {
        QByteArray data = m_pendingPackets.takeFirst();
        WatchDataReader reader(data);

        do {
            if (m_sentPackets >= m_maxTxWindow)
                break;

            int bytesToSend = (reader.size() > m_maxPacketSize ? m_maxPacketSize : reader.size());
            GATTPacket packet(GATTPacket::DATA, m_sequence, reader.readBytes(bytesToSend));
            m_service->writeCharacteristic(m_service->characteristic(writeCharacteristic), packet.data());

            m_sequence = getNextSequence(m_sequence);
            m_sentPackets++;
        } while (reader.size() != 0);

        if (m_sentPackets >= m_maxTxWindow) {
            // Add back the packet to our list so we can resume
            if (reader.size() > 0)
                m_pendingPackets.prepend(reader.readBytes(reader.size()));
            break;
        }
    }

    if (m_sentPackets >= m_maxTxWindow)
        return;
    else if (m_pendingPackets.length() > 0)
        sendDataToPebble();
}