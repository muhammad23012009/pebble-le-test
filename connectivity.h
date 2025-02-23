#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include <QObject>
#include <QFlags>
#include <QDebug>

enum ConnectivityFlag {
    CONNECTED = 0b1,
    PAIRED = 0b10,
    ENCRYPTED = 0b100
};
Q_DECLARE_FLAGS(ConnectivityFlags, ConnectivityFlag);

class Connectivity : public QObject
{
public:
    Connectivity(const QByteArray &value);

    bool connected() const;
    bool paired() const;
    bool encrypted() const;

public slots:
    void connectivityFlagsChanged(QByteArray value);

private:
    ConnectivityFlags m_flags;
};

#endif // CONNECTIVITY_H