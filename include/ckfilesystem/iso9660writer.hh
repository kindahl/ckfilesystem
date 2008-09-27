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
namespace ckFileSystem
{
	class Iso9660ImportData
	{
	public:
		unsigned char m_ucFileFlags;
		unsigned char m_ucFileUnitSize;
		unsigned char m_ucInterleaveGapSize;
		unsigned short m_usVolSeqNumber;
		unsigned long m_ulExtentLocation;
		unsigned long m_ulExtentLength;

		ckFileSystem::tDirRecordDateTime m_RecDateTime;

		Iso9660ImportData()
		{
			m_ucFileFlags = 0;
			m_ucFileUnitSize = 0;
			m_ucInterleaveGapSize = 0;
			m_usVolSeqNumber = 0;
			m_ulExtentLocation = 0;
			m_ulExtentLength = 0;

			memset(&m_RecDateTime,0,sizeof(ckFileSystem::tDirRecordDateTime));
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
		enum eSysDirType
		{
			TYPE_CURRENT,
			TYPE_PARENT
		};

		ckcore::Log *m_pLog;
		SectorOutStream *m_pOutStream;
		CSectorManager *m_pSectorManager;

		// Different standard implementations.
		Iso9660 *m_pIso9660;
		Joliet *m_pJoliet;
		ElTorito *m_pElTorito;

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
		bool WriteSysDirectory(FileTreeNode *pParent,eSysDirType Type,
			unsigned long ulDataPos,unsigned long ulDataSize);
		int WriteLocalDirEntry(ckcore::Progress &Progress,FileTreeNode *pLocalNode,
			bool bJoliet,int iLevel);
		int WriteLocalDirEntries(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
			ckcore::Progress &Progress,FileTreeNode *pLocalNode,int iLevel);

	public:
		Iso9660Writer(ckcore::Log *pLog,SectorOutStream *pOutStream,
			CSectorManager *pSectorManager,Iso9660 *pIso9660,Joliet *pJoliet,
			ElTorito *pElTorito,bool bUseFileTimes,bool bUseJoliet);
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
