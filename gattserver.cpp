#include "gattserver.h"
#include "watchdatareader.h"
#include "watchdatawriter.h"
#include <QEventLoop>
#include <QByteArray>

#include <QList>
#include <QLoggingCategory>
#include <QTimer>
#include <memory>

GATTServer::GATTServer() {
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
    connect(this, &GATTServer::dataReceived, this, &GATTServer::processData);
}

void GATTServer::processData(QByteArray data)
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

        writeToPebble(finalData);
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

        writeToPebble(finalData);
    }
}

void GATTServer::run()
{
    qDebug() << "GATT server started!";

    m_leController = QLowEnergyController::createPeripheral();

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
                resetData.append(m_maxRxWindow);
                resetData.append(m_maxTxWindow);
            }
            GATTPacket reset(GATTPacket::RESET_ACK, 0, resetData);
            m_service->writeCharacteristic(info, reset.data());
            qDebug() << "windows negotiated" << m_maxRxWindow << m_maxTxWindow;

        } else if (packet.type() == GATTPacket::ACK) {
            qDebug() << "Received ACK from watch" << packet.sequence();
            qDebug() << "pending acks are" << m_pendingAcks.keys();
            m_pendingAcks.remove(packet.sequence());

            // Send all other packets
            sendDataToPebble();

        } else if (packet.type() == GATTPacket::DATA) {
            qDebug() << "packet sequence is" << packet.sequence();
            qDebug() << "data is" << packet.data().toHex();
            if (m_remoteSequence == packet.sequence()) {
                WatchDataReader reader(packet.data());
                reader.skip(1);
                QByteArray data = reader.readBytes(packet.data().length() - 1);

                emit dataReceived(data);

                sendAck(packet.sequence());
                m_remoteSequence = getNextSequence(m_remoteSequence);
            } else {
                sendAck(m_lastAck.sequence());
            }

        } else if (packet.type() == GATTPacket::RESET) {
            // Clean up everything
            m_sequence = 0;
            m_remoteSequence = 0;
            m_pendingPackets.clear();
            m_pendingAcks.clear();

            GATTPacket reset(GATTPacket::RESET, 0, QByteArray(1, packet.version().version));
            m_service->writeCharacteristic(info, reset.data());

            m_gattVersion = packet.version();
            QByteArray resetData;
            if (m_gattVersion.supportsWindowNegotiation) {
                resetData.append(m_maxRxWindow);
                resetData.append(m_maxTxWindow);
            }
            reset = GATTPacket(GATTPacket::RESET_ACK, 0, resetData);
            m_service->writeCharacteristic(info, reset.data());
        }
    });

    m_leController->startAdvertising(QLowEnergyAdvertisingParameters(), QLowEnergyAdvertisingData());
}

void GATTServer::sendAck(int sequence)
{
    GATTPacket ack(GATTPacket::ACK, sequence, QByteArray());
    qDebug() << "do we have coalesced acking?" << m_gattVersion.supportsCoalescedAcking;
    // Coalesced ACKing doesn't work right now
//    if (!m_gattVersion.supportsCoalescedAcking) {
        m_service->writeCharacteristic(m_service->characteristic(writeCharacteristic), ack.data());
        m_lastAck = ack;
/*    } else {
        m_rxPending++;
        if (m_rxPending >= m_maxRxWindow / 2) {
            m_rxPending = 0;
            m_service->writeCharacteristic(m_service->characteristic(writeCharacteristic), ack.data());
            m_lastAck = ack;
        }
    }*/
}

void GATTServer::writeToPebble(QByteArray value)
{
    qDebug() << "Writing data to pebble!" << value.toHex() << m_pendingPackets.length();
    m_pendingPackets.append(value);
    m_pendingAcks.insert(m_sequence, GATTPacket(value));
    if (m_pendingPackets.length() == 1 && m_pendingAcks.size() == 1)
        sendDataToPebble();
}

void GATTServer::sendDataToPebble()
{
    if (m_pendingPackets.length() < m_maxTxWindow) {
        int maxPacketSize = 339 - 4;
        QByteArray data;
        while (data.length() < maxPacketSize) {
            if (m_pendingPackets.isEmpty())
                break;

            QByteArray packet = m_pendingPackets.takeFirst();

            data.append(packet);
        }

        if (data.length() > 0) {
            int bytesToSend = (data.length() > maxPacketSize ? maxPacketSize : data.length());
            WatchDataReader reader(data);
            QByteArray toSend = reader.readBytes(bytesToSend);
            GATTPacket packet(GATTPacket::DATA, m_sequence, toSend);
            m_service->writeCharacteristic(m_service->characteristic(writeCharacteristic), packet.data());
            qDebug() << "wrote data with sequence" << m_sequence;
            m_sequence = getNextSequence(m_sequence);
        }
    }
}