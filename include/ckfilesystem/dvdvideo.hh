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
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include "ckfilesystem/fileset.hh"
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/iforeader.hh"

#define DVDVIDEO_BLOCK_SIZE			2048

namespace ckFileSystem
{
	class CDvdVideo
	{
	private:
		enum eFileSetType
		{
			FST_INFO,
			FST_BACKUP,
			FST_MENU,
			FST_TITLE
		};

		ckcore::Log *m_pLog;

		ckcore::tuint64 SizeToDvdLen(ckcore::tuint64 uiFileSize);

		CFileTreeNode *FindVideoNode(CFileTree &FileTree,eFileSetType Type,unsigned long ulNumber);

		bool GetTotalTitlesSize(ckcore::tstring &FilePath,eFileSetType Type,unsigned long ulNumber,
			ckcore::tuint64 &uiFileSize);
		bool ReadFileSetInfoRoot(CFileTree &FileTree,CIfoVmgData &VmgData,
			std::vector<unsigned long> &TitleSetSectors);
		bool ReadFileSetInfo(CFileTree &FileTree,std::vector<unsigned long> &TitleSetSectors);

	public:
		CDvdVideo(ckcore::Log *pLog);
		~CDvdVideo();

		bool PrintFilePadding(CFileTree &FileTree);

		bool CalcFilePadding(CFileTree &FileTree);
	};
};
