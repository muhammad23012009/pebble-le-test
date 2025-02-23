#include <QCoreApplication>

#include "entryclass.h"
#include "packetreader.h"
#include "gattserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    PacketReader *reader = new PacketReader();
    GATTServer *server = new GATTServer(reader);

    // Now connect to pebble
    EntryClass *entry = new EntryClass(reader);

    QObject::connect(entry, &EntryClass::writeToPebble, server, &GATTServer::writeToPebble);
    QObject::connect(server, &GATTServer::dataReceived, entry, &EntryClass::dataReceived);
    QObject::connect(server, &GATTServer::connectedToPebble, entry, &EntryClass::connectedToPebble);

    server->run();

    return a.exec();
}
