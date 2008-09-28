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
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include "ckfilesystem/fileset.hh"
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/iforeader.hh"

#define DVDVIDEO_BLOCK_SIZE			2048

namespace ckfilesystem
{
	class DvdVideo
	{
	private:
		enum FileSetType
		{
			FST_INFO,
			FST_BACKUP,
			FST_MENU,
			FST_TITLE
		};

		ckcore::Log &log_;

		ckcore::tuint64 SizeToDvdLen(ckcore::tuint64 file_size);

		FileTreeNode *FindVideoNode(FileTree &file_tree,FileSetType type,unsigned long number);

		bool GetTotalTitlesSize(ckcore::tstring &file_path,FileSetType type,unsigned long number,
								ckcore::tuint64 &file_size);
		bool ReadFileSetInfoRoot(FileTree &file_tree,IfoVmgData &vmg_data,
								 std::vector<unsigned long> &ts_sectors);
		bool ReadFileSetInfo(FileTree &file_tree,std::vector<unsigned long> &ts_sectors);

	public:
		DvdVideo(ckcore::Log &log);
		~DvdVideo();

		bool PrintFilePadding(FileTree &file_tree);

		bool CalcFilePadding(FileTree &file_tree);
	};
};
