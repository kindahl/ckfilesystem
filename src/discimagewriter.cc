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

#include <ckcore/string.hh>
#include "ckfilesystem/stringtable.hh"
#include "ckfilesystem/sectormanager.hh"
#include "ckfilesystem/iso9660writer.hh"
#include "ckfilesystem/udfwriter.hh"
#include "ckfilesystem/dvdvideo.hh"
#include "ckfilesystem/discimagewriter.hh"

namespace ckfilesystem
{
	DiscImageWriter::DiscImageWriter(ckcore::Log &log,eFileSystem FileSystem) : m_FileSystem(FileSystem),
		log_(log),m_ElTorito(log),m_Udf(FileSystem == FS_DVDVIDEO)
	{
	}

	DiscImageWriter::~DiscImageWriter()
	{
	}

	/*
		Calculates file system specific data such as extent location and size for a
		single file.
	*/
	bool DiscImageWriter::CalcLocalFileSysData(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
		FileTreeNode *pLocalNode,int iLevel,ckcore::tuint64 &uiSecOffset,ckcore::Progress &Progress)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->children_.begin(); itFile !=
			pLocalNode->children_.end(); itFile++)
		{
			if ((*itFile)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (iLevel >= m_Iso9660.GetMaxDirLevel())
					continue;
				else
					DirNodeStack.push_back(std::make_pair(*itFile,iLevel + 1));
			}
			else
			{
				// Validate file size.
				if ((*itFile)->file_size_ > ISO9660_MAX_EXTENT_SIZE && !m_Iso9660.AllowsFragmentation())
				{
					if (m_FileSystem == FS_ISO9660 || m_FileSystem == FS_ISO9660_JOLIET || m_FileSystem == FS_DVDVIDEO)
					{
						log_.PrintLine(ckT("  Warning: Skipping \"%s\", the file is larger than 4 GiB."),
							(*itFile)->file_name_.c_str());
						Progress.Notify(ckcore::Progress::ckWARNING,StringTable::Instance().GetString(StringTable::WARNING_SKIP4GFILE),
							(*itFile)->file_name_.c_str());

						continue;
					}
					else if (m_FileSystem == FS_ISO9660_UDF || m_FileSystem == FS_ISO9660_UDF_JOLIET)
					{
						log_.PrintLine(ckT("  Warning: The file \"%s\" is larger than 4 GiB. It will not be visible in the ISO9660/Joliet file system."),
							(*itFile)->file_name_.c_str());
						Progress.Notify(ckcore::Progress::ckWARNING,StringTable::Instance().GetString(StringTable::WARNING_SKIP4GFILEISO),
							(*itFile)->file_name_.c_str());
					}
				}

				// If imported, use the imported information.
				if ((*itFile)->file_flags_ & FileTreeNode::FLAG_IMPORTED)
				{
					Iso9660ImportData *pImportNode = (Iso9660ImportData *)(*itFile)->data_ptr_;
					if (pImportNode == NULL)
					{
						log_.PrintLine(ckT("  Error: The file \"%s\" does not contain imported session data like advertised."),
							(*itFile)->file_name_.c_str());
						return false;
					}

					(*itFile)->data_size_normal_ = pImportNode->extent_len_;
					(*itFile)->data_size_joliet_ = pImportNode->extent_len_;

					(*itFile)->data_pos_normal_ = pImportNode->extent_loc_;
					(*itFile)->data_pos_joliet_ = pImportNode->extent_loc_;
				}
				else
				{
					(*itFile)->data_size_normal_ = (*itFile)->file_size_;
					(*itFile)->data_size_joliet_ = (*itFile)->file_size_;

					(*itFile)->data_pos_normal_ = uiSecOffset;
					(*itFile)->data_pos_joliet_ = uiSecOffset;

					uiSecOffset += (*itFile)->data_size_normal_/ISO9660_SECTOR_SIZE;
					if ((*itFile)->data_size_normal_ % ISO9660_SECTOR_SIZE != 0)
						uiSecOffset++;

					// Pad if necessary.
					uiSecOffset += (*itFile)->data_pad_len_;
				}

				/*
				(*itFile)->data_size_normal_ = (*itFile)->file_size_;
				(*itFile)->data_size_joliet_ = (*itFile)->file_size_;

				(*itFile)->data_pos_normal_ = uiSecOffset;
				(*itFile)->data_pos_joliet_ = uiSecOffset;

				uiSecOffset += (*itFile)->data_size_normal_/ISO9660_SECTOR_SIZE;
				if ((*itFile)->data_size_normal_ % ISO9660_SECTOR_SIZE != 0)
					uiSecOffset++;

				// Pad if necessary.
				uiSecOffset += (*itFile)->data_pad_len_;*/
			}
		}

		return true;
	}

	/*
		Calculates file system specific data such as location of extents and sizes of
		extents.
	*/
	bool DiscImageWriter::CalcFileSysData(FileTree &file_tree,ckcore::Progress &Progress,
		ckcore::tuint64 uiStartSec,ckcore::tuint64 &uiLastSec)
	{
		FileTreeNode *pCurNode = file_tree.GetRoot();
		ckcore::tuint64 uiSecOffset = uiStartSec;

		std::vector<std::pair<FileTreeNode *,int> > DirNodeStack;
		if (!CalcLocalFileSysData(DirNodeStack,pCurNode,0,uiSecOffset,Progress))
			return false;

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1].first;
			int iLevel = DirNodeStack[DirNodeStack.size() - 1].second;
			DirNodeStack.pop_back();

			if (!CalcLocalFileSysData(DirNodeStack,pCurNode,iLevel,uiSecOffset,Progress))
				return false;
		}

		uiLastSec = uiSecOffset;
		return true;
	}

	int DiscImageWriter::WriteFileNode(SectorOutStream &OutStream,FileTreeNode *pNode,
										ckcore::Progresser &FileProgresser)
	{
		ckcore::FileInStream FileStream(pNode->file_path_.c_str());
		if (!FileStream.Open())
		{
			log_.PrintLine(ckT("  Error: Unable to obtain file handle to \"%s\"."),
				pNode->file_path_.c_str());
			FileProgresser.Notify(ckcore::Progress::ckERROR,
				StringTable::Instance().GetString(StringTable::ERROR_OPENREAD),
				pNode->file_path_.c_str());
			return RESULT_FAIL;
		}

		if (!ckcore::stream::copy(FileStream,OutStream,FileProgresser))
		{
			log_.PrintLine(ckT("  Error: Unable write file to disc image."));
			return RESULT_FAIL;
		}

		// Pad the sector.
		if (OutStream.GetAllocated() != 0)
			OutStream.PadSector();

		return RESULT_OK;
	}

	int DiscImageWriter::WriteLocalFileData(SectorOutStream &OutStream,
		std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
		FileTreeNode *pLocalNode,int iLevel,ckcore::Progresser &FileProgresser)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->children_.begin(); itFile !=
			pLocalNode->children_.end(); itFile++)
		{
			// Check if we should abort.
			if (FileProgresser.Cancelled())
				return RESULT_CANCEL;

			if ((*itFile)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (iLevel >= m_Iso9660.GetMaxDirLevel())
					continue;
				else
					DirNodeStack.push_back(std::make_pair(*itFile,iLevel + 1));
			}
			else if (!((*itFile)->file_flags_ & FileTreeNode::FLAG_IMPORTED))	// We don't have any data to write for imported files.
			{
				// Validate file size.
				if (m_FileSystem == FS_ISO9660 || m_FileSystem == FS_ISO9660_JOLIET || m_FileSystem == FS_DVDVIDEO)
				{
					if ((*itFile)->file_size_ > ISO9660_MAX_EXTENT_SIZE && !m_Iso9660.AllowsFragmentation())
						continue;
				}

				switch (WriteFileNode(OutStream,*itFile,FileProgresser))
				{
					case RESULT_FAIL:
#ifdef _WINDOWS
						log_.PrintLine(ckT("  Error: Unable to write node \"%s\" to (%I64u,%I64u)."),
#else
						log_.PrintLine(ckT("  Error: Unable to write node \"%s\" to (%llu,%llu)."),
#endif
							(*itFile)->file_name_.c_str(),(*itFile)->data_pos_normal_,(*itFile)->data_size_normal_);
						return RESULT_FAIL;

					case RESULT_CANCEL:
						return RESULT_CANCEL;
				}

				// Pad if necessary.
				char szTemp[1] = { 0 };
				for (unsigned int i = 0; i < (*itFile)->data_pad_len_; i++)
				{
					for (unsigned int j = 0; j < ISO9660_SECTOR_SIZE; j++)
						OutStream.Write(szTemp,1);
				}
			}
		}

		return RESULT_OK;
	}

	int DiscImageWriter::WriteFileData(SectorOutStream &OutStream,FileTree &file_tree,
		ckcore::Progresser &FileProgresser)
	{
		FileTreeNode *pCurNode = file_tree.GetRoot();

		std::vector<std::pair<FileTreeNode *,int> > DirNodeStack;
		int iResult = WriteLocalFileData(OutStream,DirNodeStack,pCurNode,1,FileProgresser);
		if (iResult != RESULT_OK)
			return iResult;

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1].first;
			int iLevel = DirNodeStack[DirNodeStack.size() - 1].second;
			DirNodeStack.pop_back();

			iResult = WriteLocalFileData(OutStream,DirNodeStack,pCurNode,iLevel,FileProgresser);
			if (iResult != RESULT_OK)
				return iResult;
		}

		return RESULT_OK;
	}

	void DiscImageWriter::GetInternalPath(FileTreeNode *pChildNode,ckcore::tstring &NodePath,
		bool bExternalPath,bool bJoliet)
	{
		NodePath = ckT("/");

		if (bExternalPath)
		{
			// Joliet or ISO9660?
			if (bJoliet)
			{
#ifdef _UNICODE
				if (pChildNode->file_name_joliet_[pChildNode->file_name_joliet_.length() - 2] == ';')
					NodePath.append(pChildNode->file_name_joliet_,0,pChildNode->file_name_joliet_.length() - 2);
				else
					NodePath.append(pChildNode->file_name_joliet_);
#else
				char szAnsiName[JOLIET_MAX_NAMELEN_RELAXED + 1];
				ckcore::string::utf16_to_ansi(pChildNode->file_name_joliet_.c_str(),szAnsiName,sizeof(szAnsiName));

				if (szAnsiName[pChildNode->file_name_joliet_.length() - 2] == ';')
					szAnsiName[pChildNode->file_name_joliet_.length() - 2] = '\0';

				NodePath.append(szAnsiName);
#endif
			}
			else
			{
#ifdef _UNICODE
				wchar_t szWideName[MAX_PATH];
				ckcore::string::ansi_to_utf16(pChildNode->file_name_iso9660_.c_str(),szWideName,
											  sizeof(szWideName)/sizeof(wchar_t));

				if (szWideName[pChildNode->file_name_iso9660_.length() - 2] == ';')
					szWideName[pChildNode->file_name_iso9660_.length() - 2] = '\0';

				NodePath.append(szWideName);
#else
				if (pChildNode->file_name_iso9660_[pChildNode->file_name_iso9660_.length() - 2] == ';')
					NodePath.append(pChildNode->file_name_iso9660_,0,pChildNode->file_name_iso9660_.length() - 2);
				else
					NodePath.append(pChildNode->file_name_iso9660_);
#endif
			}
		}
		else
		{
			NodePath.append(pChildNode->file_name_);
		}

		FileTreeNode *pCurNode = pChildNode->GetParent();
		while (pCurNode->GetParent() != NULL)
		{
			if (bExternalPath)
			{
				// Joliet or ISO9660?
				if (bJoliet)
				{
	#ifdef _UNICODE
					if (pCurNode->file_name_joliet_[pCurNode->file_name_joliet_.length() - 2] == ';')
					{
						std::wstring::iterator itEnd = pCurNode->file_name_joliet_.end();
						itEnd--;
						itEnd--;

						NodePath.insert(NodePath.begin(),pCurNode->file_name_joliet_.begin(),itEnd);
					}
					else
					{
						NodePath.insert(NodePath.begin(),pCurNode->file_name_joliet_.begin(),
							pCurNode->file_name_joliet_.end());
					}
	#else
					char szAnsiName[JOLIET_MAX_NAMELEN_RELAXED + 1];
					ckcore::string::utf16_to_ansi(pCurNode->file_name_joliet_.c_str(),szAnsiName,sizeof(szAnsiName));

					if (szAnsiName[pCurNode->file_name_joliet_.length() - 2] == ';')
						szAnsiName[pCurNode->file_name_joliet_.length() - 2] = '\0';

					NodePath.insert(0,szAnsiName);
	#endif
				}
				else
				{
	#ifdef _UNICODE
					wchar_t szWideName[MAX_PATH];
					ckcore::string::ansi_to_utf16(pCurNode->file_name_iso9660_.c_str(),szWideName,
												  sizeof(szWideName)/sizeof(wchar_t));
					NodePath.insert(0,szWideName);

					if (szWideName[pCurNode->file_name_iso9660_.length() - 2] == ';')
						szWideName[pCurNode->file_name_iso9660_.length() - 2] = '\0';
	#else
					if (pCurNode->file_name_iso9660_[pCurNode->file_name_iso9660_.length() - 2] == ';')
					{
						std::string::iterator itEnd = pCurNode->file_name_iso9660_.end();
						itEnd--;
						itEnd--;

						NodePath.insert(NodePath.begin(),pCurNode->file_name_iso9660_.begin(),itEnd);
					}
					else
					{
						NodePath.insert(NodePath.begin(),pCurNode->file_name_iso9660_.begin(),
							pCurNode->file_name_iso9660_.end());
					}
	#endif
				}
			}
			else
			{
				NodePath.insert(NodePath.begin(),pCurNode->file_name_.begin(),
					pCurNode->file_name_.end());
			}

			NodePath.insert(0,ckT("/"));

			pCurNode = pCurNode->GetParent();
		}
	}

	void DiscImageWriter::CreateLocalFilePathMap(FileTreeNode *pLocalNode,
		std::vector<FileTreeNode *> &DirNodeStack,
		std::map<ckcore::tstring,ckcore::tstring> &FilePathMap,bool bJoliet)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->children_.begin(); itFile !=
			pLocalNode->children_.end(); itFile++)
		{
			if ((*itFile)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				DirNodeStack.push_back(*itFile);
			}
			else
			{
				// Yeah, this is not very efficient. Both paths should be calculated togather.
				ckcore::tstring FilePath;
				GetInternalPath(*itFile,FilePath,false,bJoliet);

				ckcore::tstring ExternalFilePath;
				GetInternalPath(*itFile,ExternalFilePath,true,bJoliet);

				FilePathMap[FilePath] = ExternalFilePath;

				//MessageBox(NULL,ExternalFilePath.c_str(),FilePath.c_str(),MB_OK);
			}
		}
	}

	/*
		Used for creating a map between the internal file names and the
		external (Joliet or ISO9660, in that order).
	*/
	void DiscImageWriter::CreateFilePathMap(FileTree &file_tree,
		std::map<ckcore::tstring,ckcore::tstring> &FilePathMap,bool bJoliet)
	{
		FileTreeNode *pCurNode = file_tree.GetRoot();

		std::vector<FileTreeNode *> DirNodeStack;
		CreateLocalFilePathMap(pCurNode,DirNodeStack,FilePathMap,bJoliet);

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack.back();
			DirNodeStack.pop_back();

			CreateLocalFilePathMap(pCurNode,DirNodeStack,FilePathMap,bJoliet);
		}
	}

	/*
		ulSectorOffset is a space assumed to be allocated before this image,
		this is used for creating multi-session discs.
		pFileNameMap is optional, it should be specified if one wants to map the
		internal file paths to the actual external paths.
	*/
	int DiscImageWriter::Create(SectorOutStream &OutStream,FileSet &Files,ckcore::Progress &Progress,
		unsigned long ulSectorOffset,std::map<ckcore::tstring,ckcore::tstring> *pFilePathMap)
	{
		log_.PrintLine(ckT("DiscImageWriter::Create"));
		log_.PrintLine(ckT("  Sector offset: %u."),ulSectorOffset);

		// The first 16 sectors are reserved for system use (write 0s).
		char szTemp[1] = { 0 };

		for (unsigned int i = 0; i < ISO9660_SECTOR_SIZE << 4; i++)
			OutStream.Write(szTemp,1);

		// ...
		/*FileSet::const_iterator itFile;
		for (itFile = Files.begin(); itFile != Files.end(); itFile++)
		{
			log_.PrintLine(ckT("  %s"),(*itFile).m_InternalPath.c_str());
		}*/
		// ...

		Progress.SetStatus(StringTable::Instance().GetString(StringTable::STATUS_BUILDTREE));
		Progress.SetMarquee(true);

		// Create a file tree.
		FileTree file_tree(log_);
		if (!file_tree.CreateFromFileSet(Files))
		{
			log_.PrintLine(ckT("  Error: Failed to build file tree."));
			return Fail(RESULT_FAIL,OutStream);
		}

		// Calculate padding if DVD-Video file system.
		if (m_FileSystem == FS_DVDVIDEO)
		{
			DvdVideo dvd_video(log_);
			if (!dvd_video.CalcFilePadding(file_tree))
			{
				log_.PrintLine(ckT("  Error: Failed to calculate file padding for DVD-Video file system."));
				return Fail(RESULT_FAIL,OutStream);
			}

			dvd_video.PrintFilePadding(file_tree);
		}

		bool bUseIso = m_FileSystem != FS_UDF;
		bool bUseUdf = m_FileSystem == FS_ISO9660_UDF || m_FileSystem == FS_ISO9660_UDF_JOLIET ||
			m_FileSystem == FS_UDF || m_FileSystem == FS_DVDVIDEO;
		bool bUseJoliet = m_FileSystem == FS_ISO9660_JOLIET || m_FileSystem == FS_ISO9660_UDF_JOLIET;

		SectorManager sec_manager(16 + ulSectorOffset);
		Iso9660Writer IsoWriter(log_,OutStream,sec_manager,m_Iso9660,m_Joliet,m_ElTorito,true,bUseJoliet);
		UdfWriter UdfWriter(log_,OutStream,sec_manager,m_Udf,true);

		int iResult = RESULT_FAIL;

		// FIXME: Put failure messages to Progress.
		if (bUseIso)
		{
			iResult = IsoWriter.AllocateHeader();
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		if (bUseUdf)
		{
			iResult = UdfWriter.AllocateHeader();
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		if (bUseIso)
		{
			iResult = IsoWriter.AllocatePathTables(Progress,Files);
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);

			iResult = IsoWriter.AllocateDirEntries(file_tree);
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		if (bUseUdf)
		{
			iResult = UdfWriter.AllocatePartition(file_tree);
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		// Allocate file data.
		ckcore::tuint64 uiFirstDataSec = sec_manager.GetNextFree();
		ckcore::tuint64 uiLastDataSec = 0;

		if (!CalcFileSysData(file_tree,Progress,uiFirstDataSec,uiLastDataSec))
		{
			log_.PrintLine(ckT("  Error: Could not calculate necessary file system information."));
			return Fail(RESULT_FAIL,OutStream);
		}

		sec_manager.AllocateDataSectors(uiLastDataSec - uiFirstDataSec);

		if (bUseIso)
		{
			iResult = IsoWriter.WriteHeader(Files,file_tree);
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		if (bUseUdf)
		{
			iResult = UdfWriter.WriteHeader();
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		// FIXME: Add progress for this.
		if (bUseIso)
		{
			iResult = IsoWriter.WritePathTables(Files,file_tree,Progress);
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);

			iResult = IsoWriter.WriteDirEntries(file_tree,Progress);
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		if (bUseUdf)
		{
			iResult = UdfWriter.WritePartition(file_tree);
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

		Progress.SetStatus(StringTable::Instance().GetString(StringTable::STATUS_WRITEDATA));
		Progress.SetMarquee(false);

		// To help keep track of the progress.
		ckcore::Progresser FileProgresser(Progress,sec_manager.GetDataLength() * ISO9660_SECTOR_SIZE);
		iResult = WriteFileData(OutStream,file_tree,FileProgresser);
		if (iResult != RESULT_OK)
			return Fail(iResult,OutStream);

		if (bUseUdf)
		{
			iResult = UdfWriter.WriteTail();
			if (iResult != RESULT_OK)
				return Fail(iResult,OutStream);
		}

#ifdef _DEBUG
		file_tree.PrintTree();
#endif

		// Map the paths if requested.
		if (pFilePathMap != NULL)
			CreateFilePathMap(file_tree,*pFilePathMap,bUseJoliet);

		OutStream.Flush();
		return RESULT_OK;
	}

	/*
		Should be called when create operation fails or cancel so that the
		broken image can be removed and the file handle closed.
	*/
	int DiscImageWriter::Fail(int iResult,SectorOutStream &OutStream)
	{
		OutStream.Flush();
		return iResult;
	}

	void DiscImageWriter::SetVolumeLabel(const ckcore::tchar *szLabel)
	{
		m_Iso9660.SetVolumeLabel(szLabel);
		m_Joliet.SetVolumeLabel(szLabel);
		m_Udf.SetVolumeLabel(szLabel);
	}

	void DiscImageWriter::SetTextFields(const ckcore::tchar *szSystem,const ckcore::tchar *szVolSetIdent,
										 const ckcore::tchar *szPublIdent,const ckcore::tchar *szPrepIdent)
	{
		m_Iso9660.SetTextFields(szSystem,szVolSetIdent,szPublIdent,szPrepIdent);
		m_Joliet.SetTextFields(szSystem,szVolSetIdent,szPublIdent,szPrepIdent);
	}

	void DiscImageWriter::SetFileFields(const ckcore::tchar *szCopyFileIdent,
										 const ckcore::tchar *szAbstFileIdent,
										 const ckcore::tchar *szBiblFileIdent)
	{
		m_Iso9660.SetFileFields(szCopyFileIdent,szAbstFileIdent,szBiblFileIdent);
		m_Joliet.SetFileFields(szCopyFileIdent,szAbstFileIdent,szBiblFileIdent);
	}

	void DiscImageWriter::SetInterchangeLevel(Iso9660::InterLevel inter_level)
	{
		m_Iso9660.SetInterchangeLevel(inter_level);
	}

	void DiscImageWriter::SetIncludeFileVerInfo(bool bIncludeInfo)
	{
		m_Iso9660.SetIncludeFileVerInfo(bIncludeInfo);
		m_Joliet.SetIncludeFileVerInfo(bIncludeInfo);
	}

	void DiscImageWriter::SetPartAccessType(Udf::PartAccessType AccessType)
	{
		m_Udf.SetPartAccessType(AccessType);
	}

	void DiscImageWriter::SetRelaxMaxDirLevel(bool bRelaxRestriction)
	{
		m_Iso9660.SetRelaxMaxDirLevel(bRelaxRestriction);
	}

	void DiscImageWriter::SetLongJolietNames(bool bEnable)
	{
		m_Joliet.SetRelaxMaxNameLen(bEnable);
	}

	bool DiscImageWriter::AddBootImageNoEmu(const ckcore::tchar *szFullPath,bool bBootable,
		unsigned short usLoadSegment,unsigned short usSectorCount)
	{
		return m_ElTorito.AddBootImageNoEmu(szFullPath,bBootable,usLoadSegment,usSectorCount);
	}

	bool DiscImageWriter::AddBootImageFloppy(const ckcore::tchar *szFullPath,bool bBootable)
	{
		return m_ElTorito.AddBootImageFloppy(szFullPath,bBootable);
	}

	bool DiscImageWriter::AddBootImageHardDisk(const ckcore::tchar *szFullPath,bool bBootable)
	{
		return m_ElTorito.AddBootImageHardDisk(szFullPath,bBootable);
	}
};
