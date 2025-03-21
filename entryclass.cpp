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

EntryClass::EntryClass(PacketReader *reader, GATTServer *server):
  m_server(server),
  m_packetReader(reader)
{
    aTimer = new QTimer(this);
    aTimer->setInterval(7 * 1000);
    aTimer->setSingleShot(true);

    connect(m_packetReader, &PacketReader::readyRead, this, &EntryClass::dataReceived, Qt::QueuedConnection);
    m_locator = new DeviceLocator();
    m_locator->setFilter("Pebble");
    connect(m_locator, &DeviceLocator::deviceAdded, this, &EntryClass::addDevice);
    connect(m_locator, &DeviceLocator::deviceRemoved, this, &EntryClass::removeDevice);
    m_locator->startDiscovery();

    connect(aTimer, &QTimer::timeout, [this]() {
        qDebug() << "Called!";

        /*QFile file("/home/thevancedgamer/test/diorite/pebble-app.bin");
        if (!file.open(QFile::ReadOnly)) {
            qWarning() << "FUCK!";
            return;
        }

        QByteArray appdata = file.read(file.size());
        WatchDataReader reader(appdata);
        qDebug() << "Header:" << reader.readFixedString(8);
        qDebug() << "Struct major version:" << reader.read<quint8>();
        qDebug() << "struct minor version:" << reader.read<quint8>();
        int sdkMajorVersion = reader.read<quint8>();
        int sdkMinorVersion = reader.read<quint8>();
        int appMajorVersion = reader.read<quint8>();
        int appMinorVersion = reader.read<quint8>();
        qDebug() << "Figure it out" << sdkMajorVersion << sdkMinorVersion << appMajorVersion << appMinorVersion;
        qDebug() << "size:" << reader.readLE<quint16>();
        qDebug() << "offset:" << reader.readLE<quint32>();
        qDebug() << "crc:" << reader.readLE<quint32>();
        QString appName = reader.readFixedString(32);
        qDebug() << "appname:" << appName << "Vendor name:" << reader.readFixedString(32);
        quint32 appIcon = reader.readLE<quint32>();
        qDebug() << "synbol thing" << reader.readLE<quint32>();
        quint32 flags = reader.readLE<quint32>();
        file.close();

        QByteArray value;
        WatchDataWriter valWriter(&value);
        valWriter.writeUuid(QUuid("85bf7c30-6d6b-451a-87d4-614d1a4dc088"));
        valWriter.writeLE<quint32>(flags);
        valWriter.writeLE<quint32>(appIcon);
        valWriter.write<quint8>(appMajorVersion);
        valWriter.write<quint8>(appMinorVersion);
        valWriter.write<quint8>(sdkMajorVersion);
        valWriter.write<quint8>(sdkMinorVersion);
        valWriter.write<quint8>(0);
        valWriter.write<quint8>(0);
        valWriter.writeFixedString(96, appName);

        QByteArray callData, finalData;
        WatchDataWriter writer(&callData);

        quint32 token = (rand() % ((int)pow(2, 16) - 2)) + 1;
        QByteArray key = QUuid("85bf7c30-6d6b-451a-87d4-614d1a4dc088").toRfc4122();
        writer.write<quint8>(1); // OperationInsert
        writer.write<quint8>(token & 0xFF);
        writer.write<quint8>((token >> 8) & 0xFF00);
        writer.write<quint8>(2); // Database, BlobDBApp

        writer.write<quint8>(key.length());
        callData.append(key);
        writer.write<quint8>(value.length() & 0xFF);
        writer.write<quint8>((value.length() >> 8) & 0xFF00);
        callData.append(value);

        finalData.append((callData.length() & 0xFF00) >> 8);
        finalData.append(callData.length() & 0xFF);
        finalData.append((45531 & 0xFF00) >> 8);
        finalData.append(45531 & 0xFF);
        finalData.append(callData);

        emit writeToPebble(finalData);*/

        QByteArray msg;
        msg.append((char)0);
        msg.append((char)0xB);
        emit writeToPebble(encodeMessage(18, msg));

        emit writeToPebble(encodeMessage(16, QByteArray(1, 0)));
    });
}

void EntryClass::addDevice(DeviceHandler *device)
{
    m_locator->stopDiscovery();

    qDebug() << "got device" << device->address() << device->name();

    m_handler = device;
    connect(m_handler, &DeviceHandler::connectedChanged, [this]() {
        if (m_handler->connected()) {
            qDebug() << "Connected to watch AAXAAXAXAXAXAXAXAXAXAXXAXAXXAXA";
            aTimer->start();
            m_server->run();
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
            QByteArray data = pairTriggers(true, false, false);
            qDebug() << "Pair triggers are" << data.toHex();

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
    //qDebug() << "Header is" << QByteArray::fromRawData(reinterpret_cast<char*>(header), 4).toHex();

    int packetLength = qFromBigEndian<quint16>(&header[0]);
    int endpoint = qFromBigEndian<quint16>(&header[2]);
    if (m_packetReader->bytesAvailable() < packetLength + 4 || packetLength == 0) {
        qDebug() << "oh shit we don't have enough bytes";
        return;
    }

    QByteArray data = m_packetReader->read(packetLength + 4);
    data = data.right(data.length() - 4);
    WatchDataReader reader(data);
    int length = packetLength;
    //qDebug() << "endpoint is" << endpoint << "and packet length is" << length;
    if (endpoint == 16) {
        reader.skip(142);
        qDebug() << "wao caps?" << reader.readLE<quint32>();
    } else if (endpoint == 17) {
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
        writer.writeLE<quint64>(2128303);


        QByteArray finalData;
        finalData.append((toSend.length() & 0xFF00) >> 8);
        finalData.append(toSend.length() & 0xFF);
        finalData.append((17 & 0xFF00) >> 8);
        finalData.append(17 & 0xFF);
        finalData.append(toSend);

        emit writeToPebble(finalData);
    } else if (endpoint == 18) {
        reader.skip(1);
        int type = reader.read<quint8>();
        if (type != 12 && type != 10) {
            qWarning() << "FUCK!" << data.toHex();
            return;
        }
        if (type == 10) {
            m_stage = 4;
            m_startTime = QDateTime::currentDateTime();
            handleUpload(QByteArray());
            return;
        }

        reader.skip(2);
        m_resource_bytes = reader.readLE<quint32>();
        int resource_crc = reader.readLE<quint32>();
        m_fw_bytes = reader.readLE<quint32>();

        if (m_fw_bytes + 1 == 974603)
            m_fw_bytes = 0;
        
        if (m_resource_bytes + 1 == 515595)
            m_resource_bytes = 0;

        bool is_zero = m_resource_bytes == 0 && m_fw_bytes == 0;
        QByteArray msg;
        WatchDataWriter writer(&msg);
        writer.write<quint8>(0);
        writer.write<quint8>(1);
        writer.writeLE<quint32>(0);
        writer.writeLE<quint32>(515595 + 974603);
        emit writeToPebble(encodeMessage(18, msg));

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
        writer.write<quint8>(1);
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
    } else if (endpoint == 8000) {
        int offset = 0;
        if (m_waitingForMore == 0) {
            quint8 resp = reader.read<quint8>();
            m_version = reader.read<quint32>();
            m_width = reader.read<quint32>();
            m_height = reader.read<quint32>();

            if (m_version == 1)
                m_waitingForMore = m_width * m_height / 8;
            else if (m_version == 2)
                m_waitingForMore = m_width * m_height;
            
            qDebug() << "meow screenshot response" << resp << m_version << m_width << m_height;
            offset = 13;
            m_acc.clear();
        }

        qDebug() << "wao" << data.length() << offset << reader.checkBad(data.length() - offset) << reader.size();
        QByteArray tmp = reader.readBytes(data.length() - offset);
        m_waitingForMore -= tmp.length();
        m_acc.append(tmp);

        qDebug() << "received bytes" << tmp.length();
        qDebug() << "remaining bytes are now" << m_waitingForMore;
        if (m_waitingForMore == 0) {
            QByteArray output;
            switch (m_version) {
            case 1: {
                int rowBytes = m_width / 8;
                for (quint32 row = 0; row < m_height; row++) {
                    for (quint32 col = 0; col < m_width; col++) {
                        char pixel = (m_acc.at(row * rowBytes + col / 8) >> (col % 8)) & 1;
                        output.append(pixel * 255);
                        output.append(pixel * 255);
                        output.append(pixel * 255);
                    }
                }
                break;
            }
            case 2:
                for (quint32 row = 0; row < m_height; row++) {
                    for (quint32 col = 0; col < m_width; col++) {
                        char pixel = m_acc.at(row * m_width + col);
                        output.append(((pixel >> 4) & 0b11) * 85);
                        output.append(((pixel >> 2) & 0b11) * 85);
                        output.append(((pixel >> 0) & 0b11) * 85);
                    }
                }
                break;
            default:
                qWarning() << "Invalid format.";
                return;
            }
    
            QImage image = QImage((uchar*)output.data(), m_width, m_height, QImage::Format_RGB888);
            QDir dir("/home/thevancedgamer/screenshots/");
            if (!dir.exists()) {
                dir.mkpath(dir.absolutePath());
            }
            QString filename = dir.absolutePath() + "/" + QDateTime::currentDateTime().toString("yyyyMMddHHmmss") + ".jpg";
            image.save(filename);
            qDebug() << "Screenshot saved to" << filename;
        }
    } else if (endpoint == 48879) {
        handleUpload(data);
    } else if (endpoint == 6001) {
        reader.read<quint8>();
        QUuid uuid = reader.readUuid();
        m_appId = reader.read<quint32>();
        qDebug() << "Received app fetch message!" << uuid << m_appId;

        QByteArray msg;
        WatchDataWriter writer(&msg);
        writer.write<quint8>(1);
        writer.write<quint8>(1);
        emit writeToPebble(encodeMessage(6001, msg));

        handleUpload(QByteArray());
    }

    if (m_packetReader->bytesAvailable() > 0)
        dataReceived();
}

QByteArray EntryClass::pairTriggers(bool supportsPinningWithoutSlaveSecurity, bool belowLollipop, bool clientMode)
{
    std::array<bool, 6> boolArray = { true, supportsPinningWithoutSlaveSecurity, false, belowLollipop, clientMode };
    QByteArray ret = boolArrayToBytes(boolArray);
    return ret;
}

void EntryClass::handleUpload(const QByteArray &data)
{
    WatchDataReader reader(data);
    int status;

    QByteArray msg;
    WatchDataWriter writer(&msg);
    qDebug() << "Recieved put bytes message, state is" << m_state;
    qDebug() << "Token is" << m_token << "ID is" << m_appId;
    int proper_size = 0;
    switch (m_state) {
        case StateNotInProgress:
            if (m_stage == 1) {
                uploadFile = new QFile("/home/thevancedgamer/test/diorite/pebble-app.bin");
                m_crc = 1466889679;
            } else if (m_stage == 2) {
                uploadFile = new QFile("/home/thevancedgamer/test/diorite/app_resources.pbpack");
                m_crc = 1909740721;
            } else if (m_stage == 3) {
                uploadFile = new QFile("/home/thevancedgamer/test/960sGtg-en_US.pbl");
                m_crc = 1644454118;
            } else if (m_stage == 4) {
                uploadFile = new QFile("/home/thevancedgamer/test/fw/system_resources.pbpack");
                m_crc = 801998790;
            } else if (m_stage == 5) {
                uploadFile = new QFile("/home/thevancedgamer/test/fw/tintin_fw.bin");
                m_crc = 143541110;
            }

            uploadFile->open(QFile::ReadOnly);

            writer.write<quint8>(Init);
            qDebug() << "Proper size is" << proper_size;
            writer.write<quint32>(uploadFile->size());
            if (m_stage == 1)      
                writer.write<quint8>(5 | 0x80);
            else if (m_stage == 2)
                writer.write<quint8>(4 | 0x80);
            else if (m_stage == 3) {
                writer.write<quint8>(6);
                writer.write<quint8>(0);
                writer.writeCString("lang");
            } else if (m_stage == 4) {
                writer.write<quint8>(3);
                writer.write<quint8>(0);
                writer.writeCString("system_resources.pbpack");
            } else if (m_stage == 5) {
                writer.write<quint8>(1);
                writer.write<quint8>(0);
                writer.writeCString("tintin_fw.bin");
            }

            if (m_stage != 3)
                writer.writeLE<quint32>(m_appId);
            
            /*if (m_resource_bytes > 0 || m_fw_bytes > 0) {
                if (m_stage == 4 || m_stage == 5) {
                    writer.write<quint32>(0xBE4354EF);
                    writer.write<quint32>(m_stage == 4 ? m_resource_bytes : m_fw_bytes);
                }
            }*/

            m_state = StateWaitForToken;
            m_remaining = uploadFile->size();
            emit writeToPebble(encodeMessage(48879, msg));
            break;

        case StateWaitForToken:
            status = reader.read<quint8>();
            m_token = reader.read<quint32>();
            m_state = StateInProgress;
            qDebug() << "Received token!" << status << m_token;
        
        case StateInProgress:
            if (m_remaining > 0) {
                QByteArray chunk = uploadFile->read(qMin<int>(m_remaining, 2000));
                writer.write<quint8>(Send);
                writer.write<quint32>(m_token);
                writer.write<quint32>(chunk.size());
                msg.append(chunk);
                emit writeToPebble(encodeMessage(48879, msg));
                m_remaining -= chunk.size();
                qDebug() << "Sending bytes" << m_remaining << uploadFile->size();
                break;
            } else {
                m_state = StateCommit;
                writer.write<quint8>(Commit);
                writer.write<quint32>(m_token);
                writer.write<quint32>(m_crc);
                qDebug() << "Commiting install" << m_token << m_crc;
                emit writeToPebble(encodeMessage(48879, msg));
                break;
            }
        case StateCommit:
            m_state = StateComplete;
            writer.write<quint8>(Complete);
            writer.write<quint32>(m_token);
            qDebug() << "Completing install" << m_token;
            emit writeToPebble(encodeMessage(48879, msg));
            break;
        case StateComplete:
            m_token = 0;
            m_state = StateNotInProgress;
            if (m_stage == 1)
                m_stage = 2;
            else if (m_stage == 2)
                m_stage = 1;
            else if (m_stage == 4)
                m_stage = 5;
            else if (m_stage == 5) {
                m_stage = 4;
                QByteArray msg;
                msg.append((char)0);
                msg.append((char)2);
                emit writeToPebble(encodeMessage(18, msg));
                qDebug() << "firmware install finished, took" << QDateTime::currentDateTime() - m_startTime;
            }
            uploadFile->close();
            uploadFile->deleteLater();
            uploadFile = nullptr;
            if (m_stage == 2 || m_stage == 5)
                handleUpload(QByteArray());
            break;
        default:
        ;
    }
}

QByteArray EntryClass::encodeMessage(int endpoint, const QByteArray &data)
{
    QByteArray output;
    output.append((data.length() & 0xFF00) >> 8);
    output.append(data.length() & 0xFF);
    output.append((endpoint & 0xFF00) >> 8);
    output.append(endpoint & 0xFF);
    output.append(data);

    return output;
}

quint32 EntryClass::stm32crc(const QByteArray &buf, quint32 crc) {
    for (int i = 0; i < buf.length(); i += 4) {
        QByteArray dw = buf.mid(i, 4);

        if (dw.length() < 4) {
            for (int j = dw.length(); j < 4; j++)
                dw.prepend('\0');

            std::reverse(dw.begin(), dw.end());
        }

        quint32 dwuint = *(quint32*)dw.constData();
        crc ^= dwuint;
        for (int j = 0; j < 32; j++) {
            crc = (crc & 0x80000000) ? ((crc << 1) ^ 0x04C11DB7) : (crc << 1);
        }
        crc &= 0xFFFFFFFF;
    }
    return crc;
}