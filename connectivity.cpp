#include "connectivity.h"

Connectivity::Connectivity(const QByteArray &value)
{
    connectivityFlagsChanged(value);
}

bool Connectivity::connected() const
{
    return m_flags & CONNECTED;
}

bool Connectivity::paired() const
{
    return m_flags & PAIRED;
}

bool Connectivity::encrypted() const
{
    return m_flags & ENCRYPTED;
}

void Connectivity::connectivityFlagsChanged(QByteArray value)
{
    if (value.length() > 0) {
        quint8 flags = value.at(0);
        m_flags = (ConnectivityFlags)flags;
    }
}