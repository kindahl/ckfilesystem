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
#include <vector>
#include <deque>
#include <ckcore/log.hh>
#include "ckfilesystem/const.hh"
#include "ckfilesystem/sectormanager.hh"
#include "ckfilesystem/sectorstream.hh"
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/udf.hh"

namespace ckfilesystem
{
	class UdfWriter : public SectorClient
	{
	private:
		// Identifiers of different sector ranges.
		enum
		{
			SR_INITIALDESCRIPTORS,
			SR_MAINDESCRIPTORS,
			SR_FILESETCONTENTS
		};

		ckcore::Log &log_;
		SectorOutStream &out_stream_;
		SectorManager &sec_manager_;

		// File system attributes.
		bool m_bUseFileTimes;

		// Different standard implementations.
		Udf &udf_;

		// Sizes of different structures.
		ckcore::tuint64 m_uiPartLength;
		tUdfExtentAd m_MainVolDescSeqExtent;
		tUdfExtentAd m_ReserveVolDescSeqExtent;

		// The time when this object was created.
		struct tm m_ImageCreate;

		// File system preparation functions.
		void CalcLocalNodeLengths(std::vector<FileTreeNode *> &DirNodeStack,
			FileTreeNode *pLocalNode);
		void CalcNodeLengths(FileTree &file_tree);

		ckcore::tuint64 CalcIdentSize(FileTreeNode *pLocalNode);
		ckcore::tuint64 CalcNodeSizeTotal(FileTreeNode *pLocalNode);
		ckcore::tuint64 CalcNodeLinksTotal(FileTreeNode *pLocalNode);
		ckcore::tuint64 CalcParitionLength(FileTree &file_tree);

		// Write functions.
		bool WriteLocalParitionDir(std::deque<FileTreeNode *> &DirNodeQueue,
			FileTreeNode *pLocalNode,unsigned long &ulCurPartSec,ckcore::tuint64 &uiUniqueIdent);
		bool WritePartitionEntries(FileTree &file_tree);

	public:
		UdfWriter(ckcore::Log &log,SectorOutStream &out_stream,
			SectorManager &sec_manager,Udf &udf,bool bUseFileTimes);
		~UdfWriter();

		int AllocateHeader();
		int AllocatePartition(FileTree &file_tree);

		int WriteHeader();
		int WritePartition(FileTree &file_tree);
		int WriteTail();
	};
};
