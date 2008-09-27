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

		Iso9660ImportData()
		{
			file_flags_ = 0;
			file_unit_size_ = 0;
			interleave_gap_size_ = 0;
			volseq_num_ = 0;
			extent_loc_ = 0;
			extent_len_ = 0;

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
		bool m_bUseJoliet;
		bool m_bUseFileTimes;

		// Sizes of different structures.
		ckcore::tuint64 m_uiPathTableSizeNormal;
		ckcore::tuint64 m_uiPathTableSizeJoliet;

		// The time when this object was created.
		struct tm m_ImageCreate;

		// File system preparation functions.
		void MakeUniqueJoliet(FileTreeNode *pNode,unsigned char *pFileName,unsigned char ucFileNameSize);
		void MakeUniqueIso9660(FileTreeNode *pNode,unsigned char *pFileName,unsigned char ucFileNameSize);

		bool CompareStrings(const char *szString1,const ckcore::tchar *szString2,unsigned char ucLength);
		bool CompareStrings(const unsigned char *pWideString1,const ckcore::tchar *szString2,unsigned char ucLength);

		bool CalcPathTableSize(FileSet &Files,bool bJolietTable,
			ckcore::tuint64 &uiPathTableSize,ckcore::Progress &Progress);
		bool CalcLocalDirEntryLength(FileTreeNode *pLocalNode,bool bJoliet,int iLevel,
			unsigned long &ulDirLength);
		bool CalcLocalDirEntriesLength(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
			FileTreeNode *pLocalNode,int iLevel,ckcore::tuint64 &uiSecOffset,ckcore::Progress &Progress);
		bool CalcDirEntriesLength(FileTree &file_tree,ckcore::Progress &Progress,
			ckcore::tuint64 uiStartSector,ckcore::tuint64 &uiLength);

		// Write functions.
		bool WritePathTable(FileSet &Files,FileTree &file_tree,bool bJolietTable,
			bool bMSBF,ckcore::Progress &Progress);
		bool WriteSysDirectory(FileTreeNode *pParent,SysDirType Type,
			unsigned long ulDataPos,unsigned long ulDataSize);
		int WriteLocalDirEntry(ckcore::Progress &Progress,FileTreeNode *pLocalNode,
			bool bJoliet,int iLevel);
		int WriteLocalDirEntries(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
			ckcore::Progress &Progress,FileTreeNode *pLocalNode,int iLevel);

	public:
		Iso9660Writer(ckcore::Log &log,SectorOutStream &out_stream,SectorManager &sec_manager,
			Iso9660 &iso9660,Joliet &joliet,ElTorito &eltorito,
			bool bUseFileTimes,bool bUseJoliet);
		~Iso9660Writer();

		int AllocateHeader();
		int AllocatePathTables(ckcore::Progress &Progress,FileSet &Files);
		int AllocateDirEntries(FileTree &file_tree,ckcore::Progress &Progress);

		int WriteHeader(FileSet &Files,FileTree &file_tree,ckcore::Progress &Progress);
		int WritePathTables(FileSet &Files,FileTree &file_tree,ckcore::Progress &Progress);
		int WriteDirEntries(FileTree &file_tree,ckcore::Progress &Progress);

		// Helper functions.
		bool ValidateTreeNode(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
			FileTreeNode *pNode,int iLevel);
		bool ValidateTree(FileTree &file_tree);
	};
};
