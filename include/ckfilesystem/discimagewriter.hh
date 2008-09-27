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
		enum eFileSystem
		{
			FS_ISO9660,
			FS_ISO9660_JOLIET,
			FS_ISO9660_UDF,
			FS_ISO9660_UDF_JOLIET,
			FS_UDF,
			FS_DVDVIDEO
		};

	private:
		ckcore::Log *m_pLog;

		// What file system should be created.
		eFileSystem m_FileSystem;

		// Different standard implementations.
		Iso9660 m_Iso9660;
		Joliet m_Joliet;
		ElTorito m_ElTorito;
		Udf m_Udf;

		bool CalcLocalFileSysData(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
			FileTreeNode *pLocalNode,int iLevel,ckcore::tuint64 &uiSecOffset,ckcore::Progress &Progress);
		bool CalcFileSysData(FileTree &file_tree,ckcore::Progress &Progress,
			ckcore::tuint64 uiStartSec,ckcore::tuint64 &uiLastSec);

		int WriteFileNode(SectorOutStream &OutStream,FileTreeNode *pNode,
			ckcore::Progresser &OutProgresser);
		int WriteLocalFileData(SectorOutStream &OutStream,
			std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
			FileTreeNode *pLocalNode,int iLevel,ckcore::Progresser &FileProgresser);
		int WriteFileData(SectorOutStream &OutStream,FileTree &file_tree,ckcore::Progresser &FileProgresser);

		void GetInternalPath(FileTreeNode *pChildNode,ckcore::tstring &NodePath,
			bool bExternalPath,bool bJoliet);
		void CreateLocalFilePathMap(FileTreeNode *pLocalNode,
			std::vector<FileTreeNode *> &DirNodeStack,
			std::map<ckcore::tstring,ckcore::tstring> &FilePathMap,bool bJoliet);
		void CreateFilePathMap(FileTree &file_tree,std::map<ckcore::tstring,ckcore::tstring> &FilePathMap,bool bJoliet);

		int Fail(int iResult,SectorOutStream &OutStream);

	public:
		DiscImageWriter(ckcore::Log *pLog,eFileSystem FileSystem);
		~DiscImageWriter();	

		int Create(SectorOutStream &OutStream,FileSet &Files,ckcore::Progress &Progress,
			unsigned long ulSectorOffset = 0,std::map<ckcore::tstring,ckcore::tstring> *pFilePathMap = NULL);

		// File system modifiers, mixed set for Joliet, UDF and ISO9660.
		void SetVolumeLabel(const ckcore::tchar *szLabel);
		void SetTextFields(const ckcore::tchar *szSystem,const ckcore::tchar *szVolSetIdent,
			const ckcore::tchar *szPublIdent,const ckcore::tchar *szPrepIdent);
		void SetFileFields(const ckcore::tchar *ucCopyFileIdent,const ckcore::tchar *ucAbstFileIdent,
			const ckcore::tchar *ucBiblIdent);
		void SetInterchangeLevel(Iso9660::InterLevel inter_level);
		void SetIncludeFileVerInfo(bool bIncludeInfo);
		void SetPartAccessType(Udf::PartAccessType AccessType);
		void SetRelaxMaxDirLevel(bool bRelaxRestriction);
		void SetLongJolietNames(bool bEnable);

		bool AddBootImageNoEmu(const ckcore::tchar *szFullPath,bool bBootable,
			unsigned short usLoadSegment,unsigned short usSectorCount);
		bool AddBootImageFloppy(const ckcore::tchar *szFullPath,bool bBootable);
		bool AddBootImageHardDisk(const ckcore::tchar *szFullPath,bool bBootable);
	};
};
