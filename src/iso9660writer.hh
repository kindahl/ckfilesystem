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
#include <string>
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include "const.hh"
#include "sectormanager.hh"
#include "fileset.hh"
#include "filetree.hh"
#include "iso9660.hh"
#include "joliet.hh"
#include "eltorito.hh"

#define ISO9660WRITER_FILENAME_BUFFER_SIZE		206			// Must be enough to hold the largest possible string using
															// any of the supported file system extensions.
namespace ckFileSystem
{
	class CIso9660ImportData
	{
	public:
		unsigned char m_ucFileFlags;
		unsigned char m_ucFileUnitSize;
		unsigned char m_ucInterleaveGapSize;
		unsigned short m_usVolSeqNumber;
		unsigned long m_ulExtentLocation;
		unsigned long m_ulExtentLength;

		ckFileSystem::tDirRecordDateTime m_RecDateTime;

		CIso9660ImportData()
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

	class CIso9660Writer : public ISectorClient
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
		CSectorOutStream *m_pOutStream;
		CSectorManager *m_pSectorManager;

		// Different standard implementations.
		CIso9660 *m_pIso9660;
		CJoliet *m_pJoliet;
		CElTorito *m_pElTorito;

		// File system attributes.
		bool m_bUseJoliet;
		bool m_bUseFileTimes;

		// Sizes of different structures.
		ckcore::tuint64 m_uiPathTableSizeNormal;
		ckcore::tuint64 m_uiPathTableSizeJoliet;

		// The time when this object was created.
		struct tm m_ImageCreate;

		// File system preparation functions.
		void MakeUniqueJoliet(CFileTreeNode *pNode,unsigned char *pFileName,unsigned char ucFileNameSize);
		void MakeUniqueIso9660(CFileTreeNode *pNode,unsigned char *pFileName,unsigned char ucFileNameSize);

		bool CompareStrings(const char *szString1,const ckcore::tchar *szString2,unsigned char ucLength);
		bool CompareStrings(const unsigned char *pWideString1,const ckcore::tchar *szString2,unsigned char ucLength);

		bool CalcPathTableSize(CFileSet &Files,bool bJolietTable,
			ckcore::tuint64 &uiPathTableSize,ckcore::Progress &Progress);
		bool CalcLocalDirEntryLength(CFileTreeNode *pLocalNode,bool bJoliet,int iLevel,
			unsigned long &ulDirLength);
		bool CalcLocalDirEntriesLength(std::vector<std::pair<CFileTreeNode *,int> > &DirNodeStack,
			CFileTreeNode *pLocalNode,int iLevel,ckcore::tuint64 &uiSecOffset,ckcore::Progress &Progress);
		bool CalcDirEntriesLength(CFileTree &FileTree,ckcore::Progress &Progress,
			ckcore::tuint64 uiStartSector,ckcore::tuint64 &uiLength);

		// Write functions.
		bool WritePathTable(CFileSet &Files,CFileTree &FileTree,bool bJolietTable,
			bool bMSBF,ckcore::Progress &Progress);
		bool WriteSysDirectory(CFileTreeNode *pParent,eSysDirType Type,
			unsigned long ulDataPos,unsigned long ulDataSize);
		int WriteLocalDirEntry(ckcore::Progress &Progress,CFileTreeNode *pLocalNode,
			bool bJoliet,int iLevel);
		int WriteLocalDirEntries(std::vector<std::pair<CFileTreeNode *,int> > &DirNodeStack,
			ckcore::Progress &Progress,CFileTreeNode *pLocalNode,int iLevel);

	public:
		CIso9660Writer(ckcore::Log *pLog,CSectorOutStream *pOutStream,
			CSectorManager *pSectorManager,CIso9660 *pIso9660,CJoliet *pJoliet,
			CElTorito *pElTorito,bool bUseFileTimes,bool bUseJoliet);
		~CIso9660Writer();

		int AllocateHeader();
		int AllocatePathTables(ckcore::Progress &Progress,CFileSet &Files);
		int AllocateDirEntries(CFileTree &FileTree,ckcore::Progress &Progress);

		int WriteHeader(CFileSet &Files,CFileTree &FileTree,ckcore::Progress &Progress);
		int WritePathTables(CFileSet &Files,CFileTree &FileTree,ckcore::Progress &Progress);
		int WriteDirEntries(CFileTree &FileTree,ckcore::Progress &Progress);

		// Helper functions.
		bool ValidateTreeNode(std::vector<std::pair<CFileTreeNode *,int> > &DirNodeStack,
			CFileTreeNode *pNode,int iLevel);
		bool ValidateTree(CFileTree &FileTree);
	};
};
