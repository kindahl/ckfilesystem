/*
 * The ckFileSystem library provides file system functionality.
 * Copyright (C) 2006-2008 Christian Kindahl
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <map>
#include <ckcore/types.hh>

namespace ckfilesystem
{
	class SectorClient
	{
	};

	class SectorManager
	{
	private:
		ckcore::tuint64 m_uiNextFreeSector;
		ckcore::tuint64 m_uiDataStart;
		ckcore::tuint64 m_uiDataLength;
		std::map<std::pair<SectorClient *,unsigned char>,ckcore::tuint64> m_ClientMap;

	public:
		SectorManager(ckcore::tuint64 uiStartSector);
		~SectorManager();

		void AllocateSectors(SectorClient *pClient,unsigned char ucIdentifier,
			ckcore::tuint64 uiNumSectors);
		void AllocateBytes(SectorClient *pClient,unsigned char ucIdentifier,
			ckcore::tuint64 uiNumBytes);

		void AllocateDataSectors(ckcore::tuint64 uiNumSectors);
		void AllocateDataBytes(ckcore::tuint64 uiNumBytes);

		ckcore::tuint64 GetStart(SectorClient *pClient,unsigned char ucIdentifier);
		ckcore::tuint64 GetNextFree();

		ckcore::tuint64 GetDataStart();
		ckcore::tuint64 GetDataLength();
	};
};

