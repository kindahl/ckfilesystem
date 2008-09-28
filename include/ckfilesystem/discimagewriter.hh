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
#include <set>
#include <map>
#include <vector>
#include <string>
#include <queue>
#include <ckcore/types.hh>
#include <ckcore/progress.hh>
#include <ckcore/log.hh>
#include "ckfilesystem/fileset.hh"
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/sectorstream.hh"
#include "ckfilesystem/iso9660.hh"
#include "ckfilesystem/joliet.hh"
#include "ckfilesystem/eltorito.hh"
#include "ckfilesystem/udf.hh"

// Fix to be able to include winbase.h.
#ifdef SetVolumeLabel
#undef SetVolumeLabel
#endif

namespace ckfilesystem
{
	class DiscImageWriter
	{
	public:
		enum FileSystem
		{
			FS_ISO9660,
			FS_ISO9660_JOLIET,
			FS_ISO9660_UDF,
			FS_ISO9660_UDF_JOLIET,
			FS_UDF,
			FS_DVDVIDEO
		};

	private:
		ckcore::Log &log_;

		// What file system should be created.
		FileSystem file_sys_;

		// Different standard implementations.
		Iso9660 iso9660_;
		Joliet joliet_;
		ElTorito eltorito_;
		Udf udf_;

		bool CalcLocalFileSysData(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
								  FileTreeNode *local_node,int level,ckcore::tuint64 &sec_offset,
								  ckcore::Progress &progress);
		bool CalcFileSysData(FileTree &file_tree,ckcore::Progress &progress,
							 ckcore::tuint64 start_sec,ckcore::tuint64 &last_sec);

		int WriteFileNode(SectorOutStream &out_stream,FileTreeNode *node,
						  ckcore::Progresser &progresser);
		int WriteLocalFileData(SectorOutStream &out_stream,
							   std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
							   FileTreeNode *local_node,int level,ckcore::Progresser &progresser);
		int WriteFileData(SectorOutStream &out_stream,FileTree &file_tree,ckcore::Progresser &progresser);

		void GetInternalPath(FileTreeNode *child_node,ckcore::tstring &node_path,
							 bool ext_path,bool joliet);
		void CreateLocalFilePathMap(FileTreeNode *local_node,
									std::vector<FileTreeNode *> &dir_node_stack,
									std::map<ckcore::tstring,ckcore::tstring> &filepath_map,
									bool joliet);
		void CreateFilePathMap(FileTree &file_tree,std::map<ckcore::tstring,ckcore::tstring> &filepath_map,
							   bool joliet);

		int Fail(int res,SectorOutStream &out_stream);

	public:
		DiscImageWriter(ckcore::Log &log,FileSystem file_sys);
		~DiscImageWriter();	

		int Create(SectorOutStream &out_stream,FileSet &files,ckcore::Progress &progress,
				   unsigned long sec_offset = 0,
				   std::map<ckcore::tstring,ckcore::tstring> *filepath_map_ptr = NULL);

		// File system modifiers, mixed set for Joliet, UDF and ISO9660.
		void SetVolumeLabel(const ckcore::tchar *label);
		void SetTextFields(const ckcore::tchar *sys_ident,
						   const ckcore::tchar *volset_ident,
						   const ckcore::tchar *publ_ident,
						   const ckcore::tchar *prep_ident);
		void SetFileFields(const ckcore::tchar *copy_file_ident,
						   const ckcore::tchar *abst_file_ident,
						   const ckcore::tchar *bibl_file_ident);
		void SetInterchangeLevel(Iso9660::InterLevel inter_level);
		void SetIncludeFileVerInfo(bool include);
		void SetPartAccessType(Udf::PartAccessType access_type);
		void SetRelaxMaxDirLevel(bool relax);
		void SetLongJolietNames(bool enable);

		bool AddBootImageNoEmu(const ckcore::tchar *full_path,bool bootable,
							   unsigned short load_segment,unsigned short sec_count);
		bool AddBootImageFloppy(const ckcore::tchar *full_path,bool bootable);
		bool AddBootImageHardDisk(const ckcore::tchar *full_path,bool bootable);
	};
};
