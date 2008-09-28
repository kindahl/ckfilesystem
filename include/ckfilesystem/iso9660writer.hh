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
#include <string>
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include "ckfilesystem/const.hh"
#include "ckfilesystem/sectormanager.hh"
#include "ckfilesystem/fileset.hh"
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/iso9660.hh"
#include "ckfilesystem/joliet.hh"
#include "ckfilesystem/eltorito.hh"

#define ISO9660WRITER_FILENAME_BUFFER_SIZE		206			// Must be enough to hold the largest possible string using
															// any of the supported file system extensions.
namespace ckfilesystem
{
	class Iso9660ImportData
	{
	public:
		unsigned char file_flags_;
		unsigned char file_unit_size_;
		unsigned char interleave_gap_size_;
		unsigned short volseq_num_;
		unsigned long extent_loc_;
		unsigned long extent_len_;

		ckfilesystem::tiso_dir_record_datetime rec_timestamp_;

		Iso9660ImportData() : file_flags_(0),file_unit_size_(0),
			interleave_gap_size_(0),volseq_num_(0),
			extent_loc_(0),extent_len_(0)
		{
			memset(&rec_timestamp_,0,sizeof(ckfilesystem::tiso_dir_record_datetime));
		}
	};

	class Iso9660Writer : public SectorClient
	{
	private:
		// Identifiers of different sector ranges.
		enum
		{
			SR_DESCRIPTORS,
			SR_BOOTCATALOG,
			SR_BOOTDATA,
			SR_PATHTABLE_NORMAL_L,
			SR_PATHTABLE_NORMAL_M,
			SR_PATHTABLE_JOLIET_L,
			SR_PATHTABLE_JOLIET_M,
			SR_DIRENTRIES
		};

		// Identifiers the two different types of system directories.
		enum SysDirType
		{
			TYPE_CURRENT,
			TYPE_PARENT
		};

		ckcore::Log &log_;
		SectorOutStream &out_stream_;
		SectorManager &sec_manager_;

		// Different standard implementations.
		Iso9660 &iso9660_;
		Joliet &joliet_;
		ElTorito &eltorito_;

		// File system attributes.
		bool use_joliet_;
		bool use_file_times_;

		// Sizes of different structures.
		ckcore::tuint64 pathtable_size_normal_;
		ckcore::tuint64 pathtable_size_joliet_;

		// The time when this object was created.
		struct tm create_time_;

		// File system preparation functions.
		void MakeUniqueJoliet(FileTreeNode *node,unsigned char *file_name_ptr,
							  unsigned char file_name_size);
		void MakeUniqueIso9660(FileTreeNode *node,unsigned char *file_name_ptr,
							   unsigned char file_name_size);

		bool CompareStrings(const char *str1,const ckcore::tchar *str2,
							unsigned char len);
		bool CompareStrings(const unsigned char *udf_str1,const ckcore::tchar *str2,
							unsigned char len);

		bool CalcPathTableSize(FileSet &files,bool joliet_table,
							   ckcore::tuint64 &pathtable_size,
							   ckcore::Progress &progress);
		bool CalcLocalDirEntryLength(FileTreeNode *local_node,bool joliet,int level,
									 unsigned long &dir_len);
		bool CalcLocalDirEntriesLength(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
									   FileTreeNode *local_node,int level,
									   ckcore::tuint64 &sec_offset);
		bool CalcDirEntriesLength(FileTree &file_tree,ckcore::tuint64 start_sec,
								  ckcore::tuint64 &len);

		// Write functions.
		bool WritePathTable(FileSet &files,FileTree &file_tree,bool joliet_table,bool msbf);
		bool WriteSysDirectory(FileTreeNode *parent_node,SysDirType type,
							   unsigned long data_pos,unsigned long data_size);
		int WriteLocalDirEntry(ckcore::Progress &progress,FileTreeNode *local_node,
							   bool joliet,int level);
		int WriteLocalDirEntries(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
								 ckcore::Progress &progress,FileTreeNode *local_node,int level);

	public:
		Iso9660Writer(ckcore::Log &log,SectorOutStream &out_stream,SectorManager &sec_manager,
					  Iso9660 &iso9660,Joliet &joliet,ElTorito &eltorito,
					  bool use_file_times,bool use_joliet);
		~Iso9660Writer();

		int AllocateHeader();
		int AllocatePathTables(ckcore::Progress &progress,FileSet &files);
		int AllocateDirEntries(FileTree &file_tree);

		int WriteHeader(FileSet &files,FileTree &file_tree);
		int WritePathTables(FileSet &files,FileTree &file_tree,ckcore::Progress &progress);
		int WriteDirEntries(FileTree &file_tree,ckcore::Progress &progress);

		// Helper functions.
		bool ValidateTreeNode(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
							  FileTreeNode *node,int level);
		bool ValidateTree(FileTree &file_tree);
	};
};
