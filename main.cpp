#include <QCoreApplication>

#include "entryclass.h"
#include "gattserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    GATTServer *server = new GATTServer();

    // Now connect to pebble
    EntryClass *entry = new EntryClass();

    server->run();

    return a.exec();
}
