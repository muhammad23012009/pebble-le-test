#ifndef PACKETREADER_H
#define PACKETREADER_H

#include <QObject>
#include <QDebug>
#include <QEventLoop>
#include <QtEndian>
#include <QIODevice>

#include "watchdatareader.h"

const int headerLength = 2 * sizeof(quint16);

class PacketReader : public QIODevice
{
    Q_OBJECT

public:
    PacketReader();

    qint64 bytesAvailable() const override;
    bool isSequential() const override;

public slots:
    void write(const QByteArray &data);

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    int m_offset = 0;
    QByteArray m_data;
};

#endif // PACKETREADER_H