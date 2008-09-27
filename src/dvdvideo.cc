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

#include <algorithm>
#include <ckcore/convert.hh>
#include "ckfilesystem/dvdvideo.hh"
#include "ckfilesystem/iforeader.hh"

namespace ckfilesystem
{
	DvdVideo::DvdVideo(ckcore::Log &log) : log_(log)
	{
	}

	DvdVideo::~DvdVideo()
	{
	}

	ckcore::tuint64 DvdVideo::SizeToDvdLen(ckcore::tuint64 uiFileSize)
	{
		return uiFileSize / DVDVIDEO_BLOCK_SIZE;
	}

	FileTreeNode *DvdVideo::FindVideoNode(FileTree &file_tree,FileSetType Type,unsigned long ulNumber)
	{
		ckcore::tstring InternalPath = ckT("/VIDEO_TS/");

		switch (Type)
		{
			case FST_INFO:
				if (ulNumber == 0)
				{
					InternalPath.append(ckT("VIDEO_TS.IFO"));
				}
				else
				{
					ckcore::tchar szFileName[13];
					ckcore::convert::sprintf(szFileName,sizeof(szFileName),ckT("VTS_%02d_0.IFO"),ulNumber);
					InternalPath.append(szFileName);
				}
				break;

			case FST_BACKUP:
				if (ulNumber == 0)
				{
					InternalPath.append(ckT("VIDEO_TS.BUP"));
				}
				else
				{
					ckcore::tchar szFileName[13];
					ckcore::convert::sprintf(szFileName,sizeof(szFileName),ckT("VTS_%02d_0.BUP"),ulNumber);
					InternalPath.append(szFileName);
				}
				break;

			case FST_MENU:
				if (ulNumber == 0)
				{
					InternalPath.append(ckT("VIDEO_TS.VOB"));
				}
				else
				{
					ckcore::tchar szFileName[13];
					ckcore::convert::sprintf(szFileName,sizeof(szFileName),ckT("VTS_%02d_0.VOB"),ulNumber);
					InternalPath.append(szFileName);
				}
				break;

			case FST_TITLE:
				{
					if (ulNumber == 0)
						return NULL;

					FileTreeNode *pLastNode = NULL;

					// We find the last title node. There may be many of them.
					ckcore::tchar szFileName[13];
					for (unsigned int i = 0; i < 9; i++)
					{
						szFileName[0] = '\0';
						ckcore::convert::sprintf(szFileName,sizeof(szFileName),ckT("VTS_%02d_%d.VOB"),ulNumber,i + 1);
						InternalPath.append(szFileName);

						FileTreeNode *pNode = file_tree.GetNodeFromPath(InternalPath.c_str());
						if (pNode == NULL)
							break;

						pLastNode = pNode;

						// Restore the full path variable.
						InternalPath = ckT("/VIDEO_TS/");
					}

					// Since we're dealing with multiple files we return immediately.
					return pLastNode;
				}

			default:
				return NULL;
		}

		return file_tree.GetNodeFromPath(InternalPath.c_str());
	}

	bool DvdVideo::GetTotalTitlesSize(ckcore::tstring &FilePath,FileSetType Type,
		unsigned long ulNumber,ckcore::tuint64 &uiFileSize)
	{
		ckcore::tstring FullPath = FilePath;

		if (ulNumber == 0)
			return false;

		uiFileSize = 0;

		ckcore::tchar szFileName[13];
		for (unsigned int i = 0; i < 9; i++)
		{
			szFileName[0] = '\0';
			ckcore::convert::sprintf(szFileName,sizeof(szFileName),ckT("VTS_%02d_%d.VOB"),ulNumber,i + 1);
			FullPath.append(szFileName);

			if (!ckcore::File::Exist(FullPath.c_str()))
				break;

			uiFileSize += ckcore::File::Size(FullPath.c_str());

			// Restore the full path variable.
			FullPath = FilePath;
		}

		return true;
	}

	bool DvdVideo::ReadFileSetInfoRoot(FileTree &file_tree,IfoVmgData &VmgData,
		std::vector<unsigned long> &TitleSetSectors)
	{
		ckcore::tuint64 uiMenuSize = 0,uiInfoSize = 0;

		FileTreeNode *pInfoNode = FindVideoNode(file_tree,FST_INFO,0);
		if (pInfoNode != NULL)
			uiInfoSize = pInfoNode->m_uiFileSize;

		FileTreeNode *pMenuNode = FindVideoNode(file_tree,FST_MENU,0);
		if (pMenuNode != NULL)
			uiMenuSize = pMenuNode->m_uiFileSize;

		FileTreeNode *pBackupNode = FindVideoNode(file_tree,FST_BACKUP,0);

		// Verify the information.
		if ((VmgData.ulLastVmgSector + 1) < (SizeToDvdLen(uiInfoSize) << 1))
		{
			log_.PrintLine(ckT("  Error: Invalid VIDEO_TS.IFO file size."));
			return false;
		}

		// Find the actuall size of .IFO.
		ckcore::tuint64 uiInfoLength = 0;
		if (pMenuNode == NULL)
		{
			if ((VmgData.ulLastVmgSector + 1) > (SizeToDvdLen(uiInfoSize) << 1))
				uiInfoLength = VmgData.ulLastVmgSector - SizeToDvdLen(uiInfoSize) + 1;
			else
				uiInfoLength = VmgData.ulLastVmgIfoSector + 1;
		}
		else
		{
			if ((VmgData.ulLastVmgIfoSector + 1) < VmgData.ulVmgMenuVobSector)
				uiInfoLength = VmgData.ulVmgMenuVobSector;
			else
				uiInfoLength = VmgData.ulLastVmgIfoSector + 1;
		}

		if (uiInfoLength > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: VIDEO_TS.IFO is larger than 4 million blocks (%I64u blocks)."),uiInfoLength);
#else
			log_.PrintLine(ckT("  Error: VIDEO_TS.IFO is larger than 4 million blocks (%llu blocks)."),uiInfoLength);
#endif 
			return false;
		}

		if (pInfoNode != NULL)
			pInfoNode->m_ulDataPadLen = (unsigned long)uiInfoLength - (unsigned long)SizeToDvdLen(uiInfoSize);

		// Find the actuall size of .VOB.
		ckcore::tuint64 uiMenuLength = 0;
		if (pMenuNode != NULL)
		{
			uiMenuLength = VmgData.ulLastVmgSector - uiInfoLength - SizeToDvdLen(uiInfoSize) + 1;

			if (uiMenuLength > 0xFFFFFFFF)
			{
#ifdef _WINDOWS
				log_.PrintLine(ckT("  Error: VIDEO_TS.VOB is larger than 4 million blocks (%I64u blocks)."),uiMenuLength);
#else
				log_.PrintLine(ckT("  Error: VIDEO_TS.VOB is larger than 4 million blocks (%lld blocks)."),uiMenuLength);
#endif
				return false;
			}

			pMenuNode->m_ulDataPadLen = (unsigned long)uiMenuLength - (unsigned long)SizeToDvdLen(uiMenuSize);
		}

		// Find the actuall size of .BUP.
		ckcore::tuint64 uiBupLength = 0;
		if (TitleSetSectors.size() > 0)
			uiBupLength = *TitleSetSectors.begin() - uiMenuLength - uiInfoLength;
		else			
			uiBupLength = VmgData.ulLastVmgSector + 1 - uiMenuLength - uiInfoLength;	// If no title sets are used.

		if (uiBupLength > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: VIDEO_TS.BUP is larger than 4 million blocks (%I64u blocks)."),uiBupLength);
#else
			log_.PrintLine(ckT("  Error: VIDEO_TS.BUP is larger than 4 million blocks (%llu blocks)."),uiBupLength);
#endif
			return false;
		}

		if (pBackupNode != NULL)
			pBackupNode->m_ulDataPadLen = (unsigned long)uiBupLength - (unsigned long)SizeToDvdLen(uiInfoSize);

		return true;
	}

	bool DvdVideo::ReadFileSetInfo(FileTree &file_tree,std::vector<unsigned long> &TitleSetSectors)
	{
		unsigned long ulCounter = 1;

		std::vector<unsigned long>::const_iterator itTitleSet;
		for (itTitleSet = TitleSetSectors.begin(); itTitleSet != TitleSetSectors.end(); itTitleSet++)
		{
			FileTreeNode *pInfoNode = FindVideoNode(file_tree,FST_INFO,ulCounter);
			if (pInfoNode == NULL)
			{
				log_.PrintLine(ckT("  Error: Unable to find IFO file in file tree."));
				return false;
			}

			IfoReader ifo_reader(pInfoNode->m_FileFullPath.c_str());
			if (!ifo_reader.Open())
			{
				log_.PrintLine(ckT("  Error: Unable to open and identify %s."),pInfoNode->m_FileName.c_str());
				return false;
			}

			if (ifo_reader.GetType() != IfoReader::IT_VTS)
			{
				log_.PrintLine(ckT("  Error: %s is not of VTS format."),pInfoNode->m_FileName.c_str());
				return false;
			}

			IfoVtsData VtsData;
			if (!ifo_reader.ReadVts(VtsData))
			{
				log_.PrintLine(ckT("  Error: Unable to read VTS data from %s."),pInfoNode->m_FileName.c_str());
				return false;
			}

			// Test if VTS_XX_0.VOB is present.
			ckcore::tuint64 uiMenuSize = 0;
			FileTreeNode *pMenuNode = FindVideoNode(file_tree,FST_MENU,ulCounter);
			if (pMenuNode != NULL)
				uiMenuSize = pMenuNode->m_uiFileSize;

			// Test if VTS_XX_X.VOB are present.
			ckcore::tuint64 uiTitleSize = 0;

			ckcore::tstring FilePath = pInfoNode->m_FileFullPath;
			FilePath.resize(FilePath.find_last_of('/') + 1);

			bool bTitle = GetTotalTitlesSize(FilePath,FST_TITLE,ulCounter,uiTitleSize);

			// Test if VTS_XX_0.IFO are present.
			ckcore::tuint64 uiInfoSize = pInfoNode->m_uiFileSize;

			// Check that the title will fit in the space given by the IFO file.
			if ((VtsData.ulLastVtsSector + 1) < (SizeToDvdLen(uiInfoSize) << 1))
			{
				log_.PrintLine(ckT("  Error: Invalid size of %s."),pInfoNode->m_FileName.c_str());
				return false;
			}
			else if (bTitle && pMenuNode != NULL && (VtsData.ulLastVtsSector + 1 < (SizeToDvdLen(uiInfoSize) << 1) +
				SizeToDvdLen(uiTitleSize) +  SizeToDvdLen(uiMenuSize)))
			{
				log_.PrintLine(ckT("  Error: Either IFO or menu VOB related to %s is of incorrect size. (1)"),
					pInfoNode->m_FileName.c_str());
				return false;
			}
			else if (bTitle && pMenuNode == NULL && (VtsData.ulLastVtsSector + 1 < (SizeToDvdLen(uiInfoSize) << 1) +
				SizeToDvdLen(uiTitleSize)))
			{
				log_.PrintLine(ckT("  Error: Either IFO or menu VOB related to %s is of incorrect size. (2)"),
					pInfoNode->m_FileName.c_str());
				return false;
			}
			else if (!bTitle && pMenuNode != NULL && (VtsData.ulLastVtsSector + 1 < (SizeToDvdLen(uiInfoSize) << 1) +
				    SizeToDvdLen(uiMenuSize)))
			{
				log_.PrintLine(ckT("  Error: Either IFO or menu VOB related to %s is of incorrect size. (3)"),
					pInfoNode->m_FileName.c_str());
				return false;
			}

			// Find the actuall size of VTS_XX_0.IFO.
			ckcore::tuint64 uiInfoLength = 0;
			if (!bTitle && pMenuNode == NULL)
			{
				uiInfoLength = VtsData.ulLastVtsSector - SizeToDvdLen(uiInfoSize) + 1;
			}
			else if (!bTitle)
			{
				uiInfoLength = VtsData.ulVtsVobSector;
			}
			else
			{
				if (VtsData.ulLastVtsIfoSector + 1 < VtsData.ulVtsMenuVobSector)
					uiInfoLength = VtsData.ulVtsMenuVobSector;
				else
					uiInfoLength = VtsData.ulLastVtsIfoSector + 1;
			}

			if (uiInfoLength > 0xFFFFFFFF)
			{
				log_.PrintLine(ckT("  Error: IFO file larger than 4 million blocks."));
				return false;
			}

			pInfoNode->m_ulDataPadLen = (unsigned long)uiInfoLength - (unsigned long)SizeToDvdLen(uiInfoSize);

			// Find the actuall size of VTS_XX_0.VOB.
			ckcore::tuint64 uiMenuLength = 0;
			if (pMenuNode != NULL)
			{
				if (bTitle && (VtsData.ulVtsVobSector - VtsData.ulVtsMenuVobSector > SizeToDvdLen(uiMenuSize)))
				{
					uiMenuLength = VtsData.ulVtsVobSector - VtsData.ulVtsMenuVobSector;
				}
				else if (!bTitle &&	(VtsData.ulVtsVobSector + SizeToDvdLen(uiMenuSize) +
					SizeToDvdLen(uiInfoSize) - 1 < VtsData.ulLastVtsSector))
				{
					uiMenuLength = VtsData.ulLastVtsSector - SizeToDvdLen(uiInfoSize) - VtsData.ulVtsMenuVobSector + 1;
				}
				else
				{
					uiMenuLength = VtsData.ulVtsVobSector - VtsData.ulVtsMenuVobSector;
				}

				if (uiMenuLength > 0xFFFFFFFF)
				{
					log_.PrintLine(ckT("  Error: Menu VOB file larger than 4 million blocks."));
					return false;
				}

				pMenuNode->m_ulDataPadLen = (unsigned long)uiMenuLength - (unsigned long)SizeToDvdLen(uiMenuSize);
			}

			// Find the actuall size of VTS_XX_[1 to 9].VOB.
			ckcore::tuint64 uiTitleLength = 0;
			if (bTitle)
			{
				uiTitleLength = VtsData.ulLastVtsSector + 1 - uiInfoLength -
					uiMenuLength - SizeToDvdLen(uiInfoSize);

				if (uiTitleLength > 0xFFFFFFFF)
				{
					log_.PrintLine(ckT("  Error: Title files larger than 4 million blocks."));
					return false;
				}

				// We only pad the last title node (not sure if that is correct).
				FileTreeNode *pLastTitleNode = FindVideoNode(file_tree,FST_TITLE,ulCounter);
				if (pLastTitleNode != NULL)
					pLastTitleNode->m_ulDataPadLen = (unsigned long)uiTitleLength - (unsigned long)SizeToDvdLen(uiTitleSize);
			}

			// Find the actuall size of VTS_XX_0.BUP.
			ckcore::tuint64 uiBupLength;
			if (TitleSetSectors.size() > ulCounter) {
				uiBupLength = TitleSetSectors[ulCounter] - TitleSetSectors[ulCounter - 1] -
					uiTitleLength - uiMenuLength - uiInfoLength;
			}
			else
			{
				uiBupLength = VtsData.ulLastVtsSector + 1 - uiTitleLength - uiMenuLength - uiInfoLength;
			}

			if (uiBupLength > 0xFFFFFFFF)
			{
				log_.PrintLine(ckT("  Error: BUP file larger than 4 million blocks."));
				return false;
			}

			FileTreeNode *pBackupNode = FindVideoNode(file_tree,FST_BACKUP,ulCounter);
			if (pBackupNode != NULL)
				pBackupNode->m_ulDataPadLen = (unsigned long)uiBupLength - (unsigned long)SizeToDvdLen(uiInfoSize);

			// We're done.
			ifo_reader.Close();

			// Increase the counter.
			ulCounter++;
		}

		return true;
	}

	bool DvdVideo::PrintFilePadding(FileTree &file_tree)
	{
		log_.PrintLine(ckT("DvdVideo::PrintFilePadding"));

		FileTreeNode *pVideoTsNode = file_tree.GetNodeFromPath(ckT("/VIDEO_TS"));
		if (pVideoTsNode == NULL)
		{
			log_.PrintLine(ckT("  Error: Unable to locate VIDEO_TS folder in file tree."));
			return false;
		}

		std::vector<FileTreeNode *>::const_iterator itVideoFile;;
		for (itVideoFile = pVideoTsNode->m_Children.begin(); itVideoFile !=
			pVideoTsNode->m_Children.end(); itVideoFile++)
		{
			log_.PrintLine(ckT("  %s: pad %u sector(s)."),
				(*itVideoFile)->m_FileName.c_str(),(*itVideoFile)->m_ulDataPadLen);
		}

		return true;
	}

	bool DvdVideo::CalcFilePadding(FileTree &file_tree)
	{
		// First locate VIDEO_TS.IFO.
		FileTreeNode *pVideoTsNode = file_tree.GetNodeFromPath(ckT("/VIDEO_TS/VIDEO_TS.IFO"));
		if (pVideoTsNode == NULL)
		{
			log_.PrintLine(ckT("  Error: Unable to locate VIDEO_TS.IFO in file tree."));
			return false;
		}

		// Read and validate VIDEO_TS.INFO.
		IfoReader ifo_reader(pVideoTsNode->m_FileFullPath.c_str());
		if (!ifo_reader.Open())
		{
			log_.PrintLine(ckT("  Error: Unable to open and identify VIDEO_TS.IFO."));
			return false;
		}

		if (ifo_reader.GetType() != IfoReader::IT_VMG)
		{
			log_.PrintLine(ckT("  Error: VIDEO_TS.IFO is not of VMG format."));
			return false;
		}

		IfoVmgData VmgData;
		if (!ifo_reader.ReadVmg(VmgData))
		{
			log_.PrintLine(ckT("  Error: Unable to read VIDEO_TS.IFO VMG data."));
			return false;
		}

		// Make a vector of all title set vectors (instead of titles).
		std::vector<unsigned long> TitleSetSectors(VmgData.Titles.size());
		std::vector<unsigned long>::const_iterator itLast =
			std::unique_copy(VmgData.Titles.begin(),VmgData.Titles.end(),TitleSetSectors.begin());
		TitleSetSectors.resize(itLast - TitleSetSectors.begin());

		// Sort the titles according to the start of the vectors.
		std::sort(TitleSetSectors.begin(),TitleSetSectors.end());

		if (!ReadFileSetInfoRoot(file_tree,VmgData,TitleSetSectors))
		{
			log_.PrintLine(ckT("  Error: Unable to obtain necessary information from VIDEO_TS.* files."));
			return false;
		}

		if (!ReadFileSetInfo(file_tree,TitleSetSectors))
		{
			log_.PrintLine(ckT("  Error: Unable to obtain necessary information from DVD-Video files."));
			return false;
		}

		return true;
	}
};
