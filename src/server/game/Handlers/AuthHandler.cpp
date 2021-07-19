/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Opcodes.h"
#include "WorldSession.h"
#include "WorldPacket.h"

void WorldSession::SendAuthResponse(uint8 code, bool queued, uint32 queuePos)
{
    WorldPacket packet(SMSG_AUTH_RESPONSE);
	
    QueryResult classResult = LoginDatabase.PQuery("SELECT class, expansion FROM realm_classes WHERE realmId = %u", realmID);
    QueryResult raceResult = LoginDatabase.PQuery("SELECT race, expansion FROM realm_races WHERE realmId = %u", realmID);

    if (!classResult || !raceResult)
    {
        sLog->outDebug(LOG_FILTER_NETWORKIO, "Unable to retrieve class or race data.");
        return;
    }
	
    packet.WriteBit(code == AUTH_OK);
	
    if (code == AUTH_OK)
    {
        packet.WriteBits(0, 21); // Send current realmId
		
        packet.WriteBits(classResult->GetRowCount(), 23);
        packet.WriteBits(0, 21);
        packet.WriteBit(0);

        packet.WriteBit(0);
        packet.WriteBit(0);
        packet.WriteBit(0);
        packet.WriteBits(raceResult->GetRowCount(), 23);
        packet.WriteBit(0);
    }

    packet.WriteBit(queued);

    if (queued)
        packet.WriteBit(1);

    packet.FlushBits();

    if (queued)
        packet << uint32(0);
	
    if (code == AUTH_OK)
    {
        do
        {
            Field* fields = raceResult->Fetch();

            packet << fields[1].GetUInt8();
            packet << fields[0].GetUInt8();
        }
        while (raceResult->NextRow());

        do
        {
            Field* fields = classResult->Fetch();

            packet << fields[1].GetUInt8();
            packet << fields[0].GetUInt8();
        }
        while (classResult->NextRow());
		
        packet << uint32(0);
        packet << uint8(Expansion());
        packet << uint32(Expansion());
        packet << uint32(0);
        packet << uint8(Expansion());
        packet << uint32(0);
        packet << uint32(0);
        packet << uint32(0);
    }

    packet << uint8(code);                             // Auth response ?

    SendPacket(&packet);
}

void WorldSession::SendClientCacheVersion(uint32 version)
{
    WorldPacket data(SMSG_CLIENT_CACHE_VERSION, 4);
    data << uint32(version);
    SendPacket(&data);
}
