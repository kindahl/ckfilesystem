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
#include <vector>
#include <deque>
#include <ckcore/log.hh>
#include "ckfilesystem/const.hh"
#include "ckfilesystem/sectormanager.hh"
#include "ckfilesystem/sectorstream.hh"
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/udf.hh"

namespace ckFileSystem
{
	class CUdfWriter : public ISectorClient
	{
	private:
		// Identifiers of different sector ranges.
		enum
		{
			SR_INITIALDESCRIPTORS,
			SR_MAINDESCRIPTORS,
			SR_FILESETCONTENTS
		};

		ckcore::Log *m_pLog;
		CSectorOutStream *m_pOutStream;
		CSectorManager *m_pSectorManager;

		// File system attributes.
		bool m_bUseFileTimes;

		// Different standard implementations.
		CUdf *m_pUdf;

		// Sizes of different structures.
		ckcore::tuint64 m_uiPartLength;
		tUdfExtentAd m_MainVolDescSeqExtent;
		tUdfExtentAd m_ReserveVolDescSeqExtent;

		// The time when this object was created.
		struct tm m_ImageCreate;

		// File system preparation functions.
		void CalcLocalNodeLengths(std::vector<CFileTreeNode *> &DirNodeStack,
			CFileTreeNode *pLocalNode);
		void CalcNodeLengths(CFileTree &FileTree);

		ckcore::tuint64 CalcIdentSize(CFileTreeNode *pLocalNode);
		ckcore::tuint64 CalcNodeSizeTotal(CFileTreeNode *pLocalNode);
		ckcore::tuint64 CalcNodeLinksTotal(CFileTreeNode *pLocalNode);
		ckcore::tuint64 CalcParitionLength(CFileTree &FileTree);

		// Write functions.
		bool WriteLocalParitionDir(std::deque<CFileTreeNode *> &DirNodeQueue,
			CFileTreeNode *pLocalNode,unsigned long &ulCurPartSec,ckcore::tuint64 &uiUniqueIdent);
		bool WritePartitionEntries(CFileTree &FileTree);

	public:
		CUdfWriter(ckcore::Log *pLog,CSectorOutStream *pOutStream,
			CSectorManager *pSectorManager,CUdf *pUdf,bool bUseFileTimes);
		~CUdfWriter();

		int AllocateHeader();
		int AllocatePartition(CFileTree &FileTree);

		int WriteHeader();
		int WritePartition(CFileTree &FileTree);
		int WriteTail();
	};
};
