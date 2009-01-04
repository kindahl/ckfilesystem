/*
 * The ckFileSystem library provides file system functionality.
 * Copyright (C) 2006-2009 Christian Kindahl
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
#include <vector>
#include <string>
#include <ckcore/types.hh>
#include <ckcore/progress.hh>
#include <ckcore/log.hh>
#include <ckcore/stream.hh>
#include "ckfilesystem/fileset.hh"
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/sectorstream.hh"
#include "ckfilesystem/iso9660.hh"
#include "ckfilesystem/joliet.hh"
#include "ckfilesystem/eltorito.hh"
#include "ckfilesystem/udf.hh"
#include "ckfilesystem/filesystem.hh"

namespace ckfilesystem
{
	class FileSystemWriter
	{
	private:
		ckcore::Log &log_;

		// What file system should be created.
		FileSystem &file_sys_;

        // File tree for caching between the write and file_path_map functions.
        FileTree file_tree_;

		bool calc_local_filesys_data(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
								     FileTreeNode *local_node,int level,ckcore::tuint64 &sec_offset,
								     ckcore::Progress &progress);
		bool calc_filesys_data(FileTree &file_tree,ckcore::Progress &progress,
						       ckcore::tuint64 start_sec,ckcore::tuint64 &last_sec);

		int write_file_node(SectorOutStream &out_stream,FileTreeNode *node,
					        ckcore::Progresser &progresser);
		int write_local_file_data(SectorOutStream &out_stream,
							      std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
							      FileTreeNode *local_node,int level,ckcore::Progresser &progresser);
		int write_file_data(SectorOutStream &out_stream,FileTree &file_tree,ckcore::Progresser &progresser);

		void get_internal_path(FileTreeNode *child_node,ckcore::tstring &node_path,
							   bool ext_path,bool joliet);
		void create_local_file_path_map(FileTreeNode *local_node,
									    std::vector<FileTreeNode *> &dir_node_stack,
									    std::map<ckcore::tstring,ckcore::tstring> &file_path_map,
									    bool joliet);
		void create_file_path_map(FileTree &file_tree,std::map<ckcore::tstring,ckcore::tstring> &file_path_map,
							      bool joliet);

		int fail(int res,SectorOutStream &out_stream);

	public:
		FileSystemWriter(ckcore::Log &log,FileSystem &file_sys);
		~FileSystemWriter();	

		int write(ckcore::OutStream &out_stream,ckcore::Progress &progress,
				  ckcore::tuint32 sec_offset = 0);

        int file_path_map(std::map<ckcore::tstring,ckcore::tstring> &file_path_map);
	};
};

