/*
 * Copyright (C) 2006-2008 Christian Kindahl, christian dot kindahl at gmail dot com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once
#include <map>
#include <ckcore/types.hh>

namespace ckFileSystem
{
	class CSectorManager;

	class ISectorClient
	{
	};

	class CSectorManager
	{
	private:
		ckcore::tuint64 m_uiNextFreeSector;
		ckcore::tuint64 m_uiDataStart;
		ckcore::tuint64 m_uiDataLength;
		std::map<std::pair<ISectorClient *,unsigned char>,ckcore::tuint64> m_ClientMap;

	public:
		CSectorManager(ckcore::tuint64 uiStartSector);
		~CSectorManager();

		void AllocateSectors(ISectorClient *pClient,unsigned char ucIdentifier,
			ckcore::tuint64 uiNumSectors);
		void AllocateBytes(ISectorClient *pClient,unsigned char ucIdentifier,
			ckcore::tuint64 uiNumBytes);

		void AllocateDataSectors(ckcore::tuint64 uiNumSectors);
		void AllocateDataBytes(ckcore::tuint64 uiNumBytes);

		ckcore::tuint64 GetStart(ISectorClient *pClient,unsigned char ucIdentifier);
		ckcore::tuint64 GetNextFree();

		ckcore::tuint64 GetDataStart();
		ckcore::tuint64 GetDataLength();
	};
};

