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
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include "ckfilesystem/fileset.hh"

namespace ckfilesystem
{
	class FileTreeNode
	{
	private:
		FileTreeNode *parent_node_;

	public:
		std::vector<FileTreeNode *> children_;

		// File information.
		enum
		{
			FLAG_DIRECTORY = 0x01,
			FLAG_IMPORTED = 0x02
		};

		unsigned char file_flags_;
		ckcore::tuint64 file_size_;
		ckcore::tstring file_name_;			// File name in disc image (requested name not actual, using ISO9660 may cripple the name).
		ckcore::tstring file_path_;			// Place on hard drive.

		// I am not sure this is the best way, this uses lots of memory.
		std::string file_name_iso9660_;
		std::wstring file_name_joliet_;

		// File system information (not set by the routines in this file).
		ckcore::tuint64 data_pos_normal_;	// Sector number of first sector containing data.
		ckcore::tuint64 data_pos_joliet_;
		ckcore::tuint64 data_size_normal_;	// Data length in bytes.
		ckcore::tuint64 data_size_joliet_;

		unsigned long data_pad_len_;		// The number of sectors to pad with zeroes after the file.

		// Sector size of UDF partition entry (all data) for an node and all it's children.
		ckcore::tuint64 udf_size_;
		ckcore::tuint64 udf_size_tot_;
		ckcore::tuint64 udf_link_tot_;		// The number of directory links within the UDF file system.
		unsigned long udf_part_loc_;		// Where is the actual UDF file entry stored.

		void *data_ptr_;					// Pointer to a user-defined structure, designed for CIso9660TreeNode

		FileTreeNode(FileTreeNode *parent_node,const ckcore::tchar *file_name,
					 const ckcore::tchar *file_path,ckcore::tuint64 file_size,
					 bool last_fragment,unsigned long fragment_index,
					 unsigned char file_flags = 0,void *data_ptr = NULL) :
			parent_node_(parent_node),
			file_flags_(file_flags),file_size_(file_size),
			file_name_(file_name),file_path_(file_path),data_ptr_(data_ptr),
			data_pos_normal_(0),data_pos_joliet_(0),
			data_size_normal_(0),data_size_joliet_(0),data_pad_len_(0),
			udf_size_(0),udf_size_tot_(0),udf_link_tot_(0),udf_part_loc_(0)
		{
		}

		~FileTreeNode()
		{
			// Free the children.
			std::vector<FileTreeNode *>::iterator it;
			for (it = children_.begin(); it != children_.end(); it++)
				delete *it;

			children_.clear();
		}

		FileTreeNode *get_parent()
		{
			return parent_node_;
		}
	};

	class FileTree
	{
	private:
		ckcore::Log &log_;
		FileTreeNode *root_node_;

		// File tree information.
		unsigned long dir_count_;
		unsigned long file_count_;

		FileTreeNode *get_child_from_file_name(FileTreeNode *parent_node,
										       const ckcore::tchar *file_name);
		bool add_file_from_path(const FileDescriptor &file);

	public:
		FileTree(ckcore::Log &log);
		~FileTree();

		FileTreeNode *get_root();
		
		bool create_from_file_set(const FileSet &files);
		FileTreeNode *get_node_from_path(const FileDescriptor &file);
		FileTreeNode *get_node_from_path(const ckcore::tchar *internal_path);

	#ifdef _DEBUG
		void print_local_tree(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
							  FileTreeNode *local_node,int indent);
		void print_tree();
	#endif

		// For obtaining file tree information.
		unsigned long get_dir_count();
		unsigned long get_file_count();
	};
};
