/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2010 MaNGOS <http://getmangos.com/>
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

#include "MMapManager.h"
#include "Log.h"
#include "World.h"

namespace MMAP
{
    // ######################## MMapManager ########################
    MMapManager::~MMapManager()
    {
        for (MMapDataSet::iterator i = loadedMMaps.begin(); i != loadedMMaps.end(); ++i)
            delete i->second;

        // by now we should not have maps loaded
        // if we had, tiles in MMapData->mmapLoadedTiles, their actual data is lost!
    }

    bool MMapManager::loadMapData(uint32 mapId)
    {
        // we already have this map loaded?
        if (loadedMMaps.find(mapId) != loadedMMaps.end())
            return true;

        // load and init dtNavMesh - read parameters from file
        uint32 pathLen = sWorld->GetDataPath().length() + strlen("mmaps/%04i.mmap")+1;
        char *fileName = new char[pathLen];
        snprintf(fileName, pathLen, (sWorld->GetDataPath()+"mmaps/%04i.mmap").c_str(), mapId);

        FILE* file = fopen(fileName, "rb");
        if (!file)
        {
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:loadMapData: Error: Could not open mmap file '%s'", fileName);
            delete [] fileName;
            return false;
        }

        dtNavMeshParams params;
        fread(&params, sizeof(dtNavMeshParams), 1, file);
        fclose(file);

        dtNavMesh* mesh = dtAllocNavMesh();
        ASSERT(mesh);
        if (DT_SUCCESS != mesh->init(&params))
        {
            dtFreeNavMesh(mesh);
            sLog->outError(LOG_FILTER_MAPS, "MMAP:loadMapData: Failed to initialize dtNavMesh for mmap %04u from file %s", mapId, fileName);
            delete [] fileName;
            return false;
        }

        delete [] fileName;

        sLog->outDebug(LOG_FILTER_MAPS, "MMAP:loadMapData: Loaded %04i.mmap", mapId);

        // store inside our map list
        MMapData* mmap_data = new MMapData(mesh);
        mmap_data->mmapLoadedTiles.clear();

        loadedMMaps.insert(std::pair<uint32, MMapData*>(mapId, mmap_data));
        return true;
    }

    uint32 MMapManager::packTileID(int32 x, int32 y)
    {
        return uint32(x << 16 | y);
    }

    bool MMapManager::loadMap(const std::string& basePath, uint32 mapId, int32 x, int32 y)
    {
        // make sure the mmap is loaded and ready to load tiles
        if (!loadMapData(mapId))
            return false;

        // get this mmap data
        MMapData* mmap = loadedMMaps[mapId];
        ASSERT(mmap->navMesh);

        // check if we already have this tile loaded
        uint32 packedGridPos = packTileID(x, y);
        if (mmap->mmapLoadedTiles.find(packedGridPos) != mmap->mmapLoadedTiles.end())
            return false;

        // load this tile :: mmaps/MMMXXYY.mmtile
        uint32 pathLen = sWorld->GetDataPath().length() + strlen("mmaps/%04i%02i%02i.mmtile")+1;
        char *fileName = new char[pathLen];
        // this change of y and x is needed because of mapbuilder.cpp (x and y are swapped there) so change it here so we dont need to re-extract. All swappes are done in other files. DONT CHANGE THIS!
        snprintf(fileName, pathLen, (sWorld->GetDataPath()+"mmaps/%04i%02i%02i.mmtile").c_str(), mapId, y, x);

        FILE *file = fopen(fileName, "rb");
        if (!file)
        {
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:loadMap: Could not open mmtile file '%s'", fileName);
            delete [] fileName;
            return false;
        }
        delete [] fileName;

        // read header
        MmapTileHeader fileHeader;
        fread(&fileHeader, sizeof(MmapTileHeader), 1, file);

        if (fileHeader.mmapMagic != MMAP_MAGIC)
        {
            sLog->outError(LOG_FILTER_MAPS, "MMAP:loadMap: Bad header in mmap %04u%02i%02i.mmtile", mapId, y, x);
            return false;
        }

        if (fileHeader.mmapVersion != MMAP_VERSION)
        {
            sLog->outError(LOG_FILTER_MAPS, "MMAP:loadMap: %04u%02i%02i.mmtile was built with generator v%i, expected v%i",
                mapId, x, y, fileHeader.mmapVersion, MMAP_VERSION);
            return false;
        }

        unsigned char* data = (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
        ASSERT(data);

        size_t result = fread(data, fileHeader.size, 1, file);
        if (!result)
        {
            sLog->outError(LOG_FILTER_MAPS, "MMAP:loadMap: Bad header or data in mmap %04u%02i%02i.mmtile", mapId, y, x);
            fclose(file);
            return false;
        }

        fclose(file);

        dtMeshHeader* header = (dtMeshHeader*)data;
        dtTileRef tileRef = 0;

        // memory allocated for data is now managed by detour, and will be deallocated when the tile is removed
        if (DT_SUCCESS == mmap->navMesh->addTile(data, fileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef))
        {
            mmap->mmapLoadedTiles.insert(std::pair<uint32, dtTileRef>(packedGridPos, tileRef));
            ++loadedTiles;
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:loadMap: Loaded mmtile %04i[%02i,%02i] into %04i[%02i,%02i]", mapId, y, x, mapId, header->x, header->y);
            return true;
        }
        else
        {
            sLog->outError(LOG_FILTER_MAPS, "MMAP:loadMap: Could not load %04u%02i%02i.mmtile into navmesh", mapId, y, x);
            dtFree(data);
            return false;
        }

        return false;
    }

    bool MMapManager::unloadMap(uint32 mapId, int32 x, int32 y)
    {
        // check if we have this map loaded
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {
            // file may not exist, therefore not loaded
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMap: Asked to unload not loaded navmesh map. %04u%02i%02i.mmtile", mapId, y, x);
            return false;
        }

        MMapData* mmap = loadedMMaps[mapId];

        // check if we have this tile loaded
        uint32 packedGridPos = packTileID(x, y);
        if (mmap->mmapLoadedTiles.find(packedGridPos) == mmap->mmapLoadedTiles.end())
        {
            // file may not exist, therefore not loaded
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMap: Asked to unload not loaded navmesh tile. %04u%02i%02i.mmtile", mapId, y, x);
            return false;
        }

        dtTileRef tileRef = mmap->mmapLoadedTiles[packedGridPos];

        // unload, and mark as non loaded
        if (DT_SUCCESS != mmap->navMesh->removeTile(tileRef, NULL, NULL))
        {
            // this is technically a memory leak
            // if the grid is later reloaded, dtNavMesh::addTile will return error but no extra memory is used
            // we cannot recover from this error - assert out
            sLog->outError(LOG_FILTER_MAPS, "MMAP:unloadMap: Could not unload %04u%02i%02i.mmtile from navmesh", mapId, y, x);
            ASSERT(false);
        }
        else
        {
            mmap->mmapLoadedTiles.erase(packedGridPos);
            --loadedTiles;
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMap: Unloaded mmtile %04i[%02i,%02i] from %04i", mapId, y, x, mapId);
            return true;
        }

        return false;
    }

    bool MMapManager::unloadMap(uint32 mapId)
    {
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {
            // file may not exist, therefore not loaded
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMap: Asked to unload not loaded navmesh map %04u", mapId);
            return false;
        }

        // unload all tiles from given map
        MMapData* mmap = loadedMMaps[mapId];
        for (MMapTileSet::iterator i = mmap->mmapLoadedTiles.begin(); i != mmap->mmapLoadedTiles.end(); ++i)
        {
            uint32 x = (i->first >> 16);
            uint32 y = (i->first & 0x0000FFFF);
            if (DT_SUCCESS != mmap->navMesh->removeTile(i->second, NULL, NULL))
                sLog->outError(LOG_FILTER_MAPS, "MMAP:unloadMap: Could not unload %04u%02i%02i.mmtile from navmesh", mapId, y, x);
            else
            {
                --loadedTiles;
                sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMap: Unloaded mmtile %04i[%02i,%02i] from %04i", mapId, y, x, mapId);
            }
        }

        delete mmap;
        loadedMMaps.erase(mapId);
        sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMap: Unloaded %04i.mmap", mapId);

        return true;
    }

    bool MMapManager::unloadMapInstance(uint32 mapId, uint32 instanceId)
    {
        // check if we have this map loaded
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
        {
            // file may not exist, therefore not loaded
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMapInstance: Asked to unload not loaded navmesh map %04u", mapId);
            return false;
        }

        MMapData* mmap = loadedMMaps[mapId];
        if (mmap->navMeshQueries.find(instanceId) == mmap->navMeshQueries.end())
        {
            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMapInstance: Asked to unload not loaded dtNavMeshQuery mapId %04u instanceId %u", mapId, instanceId);
            return false;
        }

        dtNavMeshQuery* query = mmap->navMeshQueries[instanceId];

        dtFreeNavMeshQuery(query);
        mmap->navMeshQueries.erase(instanceId);
        sLog->outDebug(LOG_FILTER_MAPS, "MMAP:unloadMapInstance: Unloaded mapId %04u instanceId %u", mapId, instanceId);

        return true;
    }

    dtNavMesh const* MMapManager::GetNavMesh(uint32 mapId)
    {
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
            return NULL;

        return loadedMMaps[mapId]->navMesh;
    }

    dtNavMeshQuery const* MMapManager::GetNavMeshQuery(uint32 mapId, uint32 instanceId)
    {
        if (loadedMMaps.find(mapId) == loadedMMaps.end())
            return NULL;

        MMapData* mmap = loadedMMaps[mapId];
        if (mmap->navMeshQueries.find(instanceId) == mmap->navMeshQueries.end())
        {
            // allocate mesh query
            dtNavMeshQuery* query = dtAllocNavMeshQuery();
            ASSERT(query);
            if (DT_SUCCESS != query->init(mmap->navMesh, 1024))
            {
                dtFreeNavMeshQuery(query);
                sLog->outError(LOG_FILTER_MAPS, "MMAP:GetNavMeshQuery: Failed to initialize dtNavMeshQuery for mapId %04u instanceId %u", mapId, instanceId);
                return NULL;
            }

            sLog->outDebug(LOG_FILTER_MAPS, "MMAP:GetNavMeshQuery: created dtNavMeshQuery for mapId %04u instanceId %u", mapId, instanceId);
            mmap->navMeshQueries.insert(std::pair<uint32, dtNavMeshQuery*>(instanceId, query));
        }

        return mmap->navMeshQueries[instanceId];
    }
}
