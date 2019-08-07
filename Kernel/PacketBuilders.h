
#ifndef PacketBuilders_h__
#define PacketBuilders_h__

#include "Criteria.h"
#include <Opcodes.h>
#include <Packet.h>

static inline Packet* BuildSelect(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_SELECT, 20);
    Packet_Append(p, dbr->TableName);
    Packet_Append(p, dbr->Data.Select.Key);
    return p;
}

static inline Packet* BuildInsert(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_INSERT, 40);
    Packet_Append(p, dbr->TableName);
    Packet_Append(p, dbr->Data.Insert.Key);
    Packet_Append(p, dbr->Data.Insert.Value);
    return p;
}

static inline Packet* BuildCreate(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_CREATE, 23);
    Packet_Append(p, dbr->TableName);
    Packet_Append(p, (uint8_t) dbr->Data.Create.Consistency);
    Packet_Append(p, dbr->Data.Create.Partitions);
    Packet_Append(p, dbr->Data.Create.CompactTime);
    return p;
}

static inline Packet* BuildDescribe(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_DESCRIBE, 16);
    {
        char const* tableName = dbr->TableName;
        Packet_Append(p, (bool) tableName);
        if (tableName)
            Packet_Append(p, tableName);
    }
    return p;
}

static inline Packet* BuildDrop(DBRequest const* dbr)
{
    Packet* p = Packet_Create(LQL_DROP, 16);
    Packet_Append(p, dbr->TableName);
    return p;
}

static inline Packet* BuildJournal(DBRequest const* dbr)
{
    (void) dbr;
    return Packet_Create(LQL_JOURNAL, 0);
}

#endif //PacketBuilders_h__
