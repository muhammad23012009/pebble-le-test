#include "watchdatawriter.h"

void WatchDataWriter::writeBytes(int n, const QByteArray &b)
{
    if (b.size() > n) {
        _buf->append(b.constData(), n);
    } else {
        int diff = n - b.size();
        _buf->append(b);
        if (diff > 0) {
            _buf->append(QByteArray(diff, '\0'));
        }
    }
}

void WatchDataWriter::writeFixedString(int n, const QString &s)
{
    _buf->append(s.left(n).toUtf8());
    for (int i = s.left(n).length(); i < n; i++) {
        _buf->append('\0');
    }
}

void WatchDataWriter::writeCString(const QString &s)
{
    _buf->append(s.toUtf8());
    _buf->append('\0');
}

void WatchDataWriter::writePascalString(const QString &s)
{
    _buf->append(s.length());
    _buf->append(s.toLatin1());
}

void WatchDataWriter::writeUuid(const QUuid &uuid)
{
    writeBytes(16, uuid.toRfc4122());
}
