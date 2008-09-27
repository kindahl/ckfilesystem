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

#include <time.h>
#include <ckcore/file.hh>
#include <ckcore/directory.hh>
#include <ckcore/convert.hh>
#include "ckfilesystem/stringtable.hh"
#include "ckfilesystem/iso9660reader.hh"
#include "ckfilesystem/iso9660writer.hh"

namespace ckFileSystem
{
	Iso9660Writer::Iso9660Writer(ckcore::Log *pLog,SectorOutStream *pOutStream,
		CSectorManager *pSectorManager,Iso9660 *pIso9660,Joliet *pJoliet,
		ElTorito *pElTorito,bool bUseFileTimes,bool bUseJoliet) :
		m_pLog(pLog),m_pOutStream(pOutStream),m_pSectorManager(pSectorManager),
		m_pIso9660(pIso9660),m_pJoliet(pJoliet),m_pElTorito(pElTorito),
		m_bUseFileTimes(bUseFileTimes),m_bUseJoliet(bUseJoliet),
		m_uiPathTableSizeNormal(0),m_uiPathTableSizeJoliet(0)
	{
		// Get system time.
        time_t CurrentTime;
        time(&CurrentTime);

        m_ImageCreate = *localtime(&CurrentTime);
	}

	Iso9660Writer::~Iso9660Writer()
	{
	}

	/**
		Tries to assure that the specified file name is unqiue. Only 255 collisions
		are supported. If any more collisions occur duplicate file names will be
		written to the file system.
	*/
	void Iso9660Writer::MakeUniqueJoliet(FileTreeNode *pNode,unsigned char *pFileName,unsigned char ucFileNameSize)
	{
		FileTreeNode *pParentNode = pNode->GetParent();
		if (pParentNode == NULL)
			return;

		// Don't calculate a new name of one has already been generated.
		if (pNode->m_FileNameJoliet != L"")
		{
			unsigned char ucFileNamePos = 0;
			for (unsigned int i = 0; i < ucFileNameSize; i += 2,ucFileNamePos++)
			{
				pFileName[i    ] = pNode->m_FileNameJoliet[ucFileNamePos] >> 8;
				pFileName[i + 1] = pNode->m_FileNameJoliet[ucFileNamePos] & 0xFF;
			}

			return;
		}

		unsigned char ucFileNameLen = ucFileNameSize >> 1;
		unsigned char ucFileNamePos = 0;

		wchar_t szFileName[(ISO9660WRITER_FILENAME_BUFFER_SIZE >> 1) + 1];
		for (unsigned int i = 0; i < ucFileNameLen; i++)
		{
			szFileName[i]  = pFileName[ucFileNamePos++] << 8;
			szFileName[i] |= pFileName[ucFileNamePos++];
		}

		szFileName[ucFileNameLen] = '\0';

		// We're only interested in the file name without the extension and
		// version information.
		int iDelimiter = -1;
		for (unsigned int i = 0; i < ucFileNameLen; i++)
		{
			if (szFileName[i] == '.')
				iDelimiter = i;
		}

		unsigned char ucFileNameEnd;

		// If no '.' character was found just remove the version information if
		// we're dealing with a file.
		if (iDelimiter == -1)
		{
			if (!(pNode->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY) &&
				m_pJoliet->IncludesFileVerInfo())
				ucFileNameEnd = ucFileNameLen - 2;
			else
				ucFileNameEnd = ucFileNameLen;
		}
		else
		{
			ucFileNameEnd = iDelimiter;
		}

		// We can't handle file names shorted than 3 characters (255 limit).
		if (ucFileNameEnd <= 3)
		{
			pNode->m_FileNameJoliet = szFileName;

			// Copy back to original buffer.
			ucFileNamePos = 0;
			for (unsigned int i = 0; i < ucFileNameSize; i += 2,ucFileNamePos++)
			{
				pFileName[i    ] = szFileName[ucFileNamePos] >> 8;
				pFileName[i + 1] = szFileName[ucFileNamePos] & 0xFF;
			}

			return;
		}

		unsigned char ucNextNumber = 1;
		wchar_t szNextNumber[4];

		std::vector<FileTreeNode *>::const_iterator itSibling;
		for (itSibling = pParentNode->m_Children.begin(); itSibling != pParentNode->m_Children.end();)
		{
			// Ignore any siblings that has not yet been assigned an ISO9660 compatible file name.
			if ((*itSibling)->m_FileNameIso9660 == "")
			{
				itSibling++;
				continue;
			}

			if (!wcscmp((*itSibling)->m_FileNameJoliet.c_str(),szFileName))
			{
				swprintf(szNextNumber,4,L"%d",ucNextNumber);

				// Using if-statements for optimization.
				if (ucNextNumber < 10)
					szFileName[ucFileNameEnd - 1] = szNextNumber[0];
				else if (ucNextNumber < 100)
					memcpy(szFileName + ucFileNameEnd - 2,szNextNumber,2 * sizeof(wchar_t));
				else
					memcpy(szFileName + ucFileNameEnd - 3,szNextNumber,3 * sizeof(wchar_t));

				if (ucNextNumber == 255)
				{
					// We have failed, files with duplicate names will exist.
					m_pLog->PrintLine(ckT("  Warning: Unable to calculate unique Joliet name for %s. Duplicate file names will exist in Joliet name extension."),
						pNode->m_FileFullPath.c_str());
					break;
				}

				// Start from the beginning.
				ucNextNumber++;
				itSibling = pParentNode->m_Children.begin();
				continue;
			}

			itSibling++;
		}

		pNode->m_FileNameJoliet = szFileName;

		// Copy back to original buffer.
		ucFileNamePos = 0;
		for (unsigned int i = 0; i < ucFileNameSize; i += 2,ucFileNamePos++)
		{
			pFileName[i    ] = szFileName[ucFileNamePos] >> 8;
			pFileName[i + 1] = szFileName[ucFileNamePos] & 0xFF;
		}
	}

	/**
		Tries to assure that the specified file name is unqiue. Only 255 collisions
		are supported. If any more collisions occur duplicate file names will be
		written to the file system.
	*/
	void Iso9660Writer::MakeUniqueIso9660(FileTreeNode *pNode,unsigned char *pFileName,unsigned char ucFileNameSize)
	{
		FileTreeNode *pParentNode = pNode->GetParent();
		if (pParentNode == NULL)
			return;

		// Don't calculate a new name of one has already been generated.
		if (pNode->m_FileNameIso9660 != "")
		{
			memcpy(pFileName,pNode->m_FileNameIso9660.c_str(),ucFileNameSize);
			return;
		}

		// We're only interested in the file name without the extension and
		// version information.
		int iDelimiter = -1;
		for (unsigned int i = 0; i < ucFileNameSize; i++)
		{
			if (pFileName[i] == '.')
				iDelimiter = i;
		}

		unsigned char ucFileNameEnd;

		// If no '.' character was found just remove the version information if
		// we're dealing with a file.
		if (iDelimiter == -1)
		{
			if (!(pNode->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY) &&
				m_pIso9660->IncludesFileVerInfo())
				ucFileNameEnd = ucFileNameSize - 2;
			else
				ucFileNameEnd = ucFileNameSize;
		}
		else
		{
			ucFileNameEnd = iDelimiter;
		}

		// We can't handle file names shorted than 3 characters (255 limit).
		if (ucFileNameEnd <= 3)
		{
			char szFileName[ISO9660WRITER_FILENAME_BUFFER_SIZE + 1];
			memcpy(szFileName,pFileName,ucFileNameSize);
			szFileName[ucFileNameSize] = '\0';

			pNode->m_FileNameIso9660 = szFileName;

			// Copy back to original buffer.
			memcpy(pFileName,szFileName,ucFileNameSize);
			return;
		}

		unsigned char ucNextNumber = 1;
		char szNextNumber[4];

		std::vector<FileTreeNode *>::const_iterator itSibling;
		for (itSibling = pParentNode->m_Children.begin(); itSibling != pParentNode->m_Children.end();)
		{
			// Ignore any siblings that has not yet been assigned an ISO9660 compatible file name.
			if ((*itSibling)->m_FileNameIso9660 == "")
			{
				itSibling++;
				continue;
			}

			if (!memcmp((*itSibling)->m_FileNameIso9660.c_str(),pFileName,/*ucFileNameSize*/ucFileNameEnd))
			{
				sprintf(szNextNumber,"%d",ucNextNumber);

				// Using if-statements for optimization.
				if (ucNextNumber < 10)
					pFileName[ucFileNameEnd - 1] = szNextNumber[0];
				else if (ucNextNumber < 100)
					memcpy(pFileName + ucFileNameEnd - 2,szNextNumber,2);
				else
					memcpy(pFileName + ucFileNameEnd - 3,szNextNumber,3);

				if (ucNextNumber == 255)
				{
					// We have failed, files with duplicate names will exist.
					m_pLog->PrintLine(ckT("  Warning: Unable to calculate unique ISO9660 name for %s. Duplicate file names will exist in ISO9660 file system."),
						pNode->m_FileFullPath.c_str());
					break;
				}

				// Start from the beginning.
				ucNextNumber++;
				itSibling = pParentNode->m_Children.begin();
				continue;
			}

			itSibling++;
		}

		char szFileName[ISO9660WRITER_FILENAME_BUFFER_SIZE + 1];
		memcpy(szFileName,pFileName,ucFileNameSize);
		szFileName[ucFileNameSize] = '\0';

		pNode->m_FileNameIso9660 = szFileName;

		// Copy back to original buffer.
		memcpy(pFileName,szFileName,ucFileNameSize);
	}

	bool Iso9660Writer::CompareStrings(const char *szString1,const ckcore::tchar *szString2,unsigned char ucLength)
	{
#ifdef _WINDOWS
#ifdef _UNICODE
		for (unsigned char i = 0; i < ucLength; i++)
			if (toupper(szString1[i]) != toupper(szString2[i]))
				return false;

		return true;
#else
		return !_strnicmp(szString1,szString2,ucLength);
#endif
#else   
        return !strncasecmp(szString1,szString2,ucLength);
#endif
	}

	bool Iso9660Writer::CompareStrings(const unsigned char *pWideString1,const ckcore::tchar *szString2,unsigned char ucLength)
	{
#ifdef _UNICODE
		unsigned char ucFileNamePos = 0;
		for (unsigned int i = 0; i < ucLength; i++)
		{
			wchar_t c = pWideString1[ucFileNamePos++] << 8;
			c |= pWideString1[ucFileNamePos++];

			if (towupper(c) != towupper(szString2[i]))
				return false;
		}

		return true;
#else
		unsigned char ucFileNamePos = 0;
		for (unsigned int i = 0; i < ucLength; i++)
		{
			wchar_t c = pWideString1[ucFileNamePos++] << 8;
			c |= pWideString1[ucFileNamePos++];

			if (towupper(c) != toupper(szString2[i]))
				return false;
		}
		return true;
#endif
	}

	bool Iso9660Writer::CalcPathTableSize(FileSet &Files,bool bJolietTable,
		ckcore::tuint64 &uiPathTableSize,ckcore::Progress &Progress)
	{
		// Root record + 1 padding byte since the root record size is odd.
		uiPathTableSize = sizeof(tPathTableRecord) + 1;

		// Write all other path table records.
		std::set<ckcore::tstring> PathDirList;		// To help keep track of which records that have already been counted.
		ckcore::tstring PathBuffer,CurDirName,InternalPath;

		// Set to true of we have found that the directory structure is to deep.
		// This variable is needed so that the warning message will only be printed
		// once.
		bool bFoundDeep = false;

		FileSet::const_iterator itFile;
		for (itFile = Files.begin(); itFile != Files.end(); itFile++)
		{
			// We're do not have to add the root record once again.
			int iLevel = FileComparator::Level(*itFile);
			if (iLevel <= 1)
				continue;

			if (iLevel > m_pIso9660->GetMaxDirLevel())
			{
				// Print the message only once.
				if (!bFoundDeep)
				{
					m_pLog->PrintLine(ckT("  Warning: The directory structure is deeper than %d levels. Deep files and folders will be ignored."),
						m_pIso9660->GetMaxDirLevel());
					Progress.Notify(ckcore::Progress::ckWARNING,g_StringTable.GetString(WARNING_FSDIRLEVEL),m_pIso9660->GetMaxDirLevel());
					bFoundDeep = true;
				}

				m_pLog->PrintLine(ckT("  Skipping: %s."),itFile->m_InternalPath.c_str());
				Progress.Notify(ckcore::Progress::ckWARNING,g_StringTable.GetString(WARNING_SKIPFILE),
					itFile->m_InternalPath.c_str());
				continue;
			}

			// We're only interested in directories.
			if (!(itFile->m_ucFlags & FileDescriptor::FLAG_DIRECTORY))
				continue;

			// Make sure that the path to the current file or folder exists.
			size_t iPrevDelim = 0;

			// If the file is a directory, append a slash so it will be parsed as a
			// directory in the loop below.
			InternalPath = itFile->m_InternalPath;
			InternalPath.push_back('/');

			PathBuffer.erase();
			PathBuffer = ckT("/");

			for (size_t i = 0; i < InternalPath.length(); i++)
			{
				if (InternalPath[i] == '/')
				{
					if (i > (iPrevDelim + 1))
					{
						// Obtain the name of the current directory.
						CurDirName.erase();
						for (size_t j = iPrevDelim + 1; j < i; j++)
							CurDirName.push_back(InternalPath[j]);

						PathBuffer += CurDirName;
						PathBuffer += ckT("/");

						// The path does not exist, create it.
						if (PathDirList.find(PathBuffer) == PathDirList.end())
						{
							unsigned char ucNameLen = bJolietTable ?
								m_pJoliet->CalcFileNameLen(CurDirName.c_str(),true) << 1 : 
								m_pIso9660->CalcFileNameLen(CurDirName.c_str(),true);
							unsigned char ucPathTableRecLen = sizeof(tPathTableRecord) + ucNameLen - 1;

							// If the record length is not even padd it with a 0 byte.
							if (ucPathTableRecLen % 2 == 1)
								ucPathTableRecLen++;

							PathDirList.insert(PathBuffer);

							// Update the path table length.
							uiPathTableSize += ucPathTableRecLen;
						}
					}

					iPrevDelim = i;
				}
			}
		}

		return true;
	}

	/*
		Calculates the size of a directory entry in sectors. This size does not include the size
		of the extend data of the directory contents.
	*/
	bool Iso9660Writer::CalcLocalDirEntryLength(FileTreeNode *pLocalNode,bool bJoliet,
		int iLevel,unsigned long &ulDirLength)
	{
		ulDirLength = 0;

		// The number of bytes of data in the current sector.
		// Each directory always includes '.' and '..'.
		unsigned long ulDirSecData = sizeof(tDirRecord) << 1;

		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (iLevel >= m_pIso9660->GetMaxDirLevel())
					continue;
			}

			// Validate file size.
			if ((*itFile)->m_uiFileSize > ISO9660_MAX_EXTENT_SIZE && !m_pIso9660->AllowsFragmentation())
				continue;

			// Calculate the number of times this record will be written. It
			// will be larger than one when using multi-extent.
			unsigned long ulFactor = 1;
			ckcore::tuint64 uiExtentRemain = (*itFile)->m_uiFileSize;
			while (uiExtentRemain > ISO9660_MAX_EXTENT_SIZE)
			{
				uiExtentRemain -= ISO9660_MAX_EXTENT_SIZE;
				ulFactor++;
			}

			bool bIsFolder = (*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY;

			unsigned char ucNameLen;	// FIXME: Rename to Size?
			/*if (bJoliet)
				ucNameLen = m_pJoliet->CalcFileNameLen((*itFile)->m_FileName.c_str(),bIsFolder) << 1;
			else
				ucNameLen = m_pIso9660->CalcFileNameLen((*itFile)->m_FileName.c_str(),bIsFolder);*/
			
			// Optimization: If the the compatible file name is equal (not case-sensitive) to the
			// original file name we assume that it is uniqe.
			unsigned char szFileName[ISO9660WRITER_FILENAME_BUFFER_SIZE + 4]; // Large enough for level 1, 2 and even Joliet.
			if (bJoliet)
			{
				ucNameLen = m_pJoliet->WriteFileName(szFileName,(*itFile)->m_FileName.c_str(),bIsFolder) << 1;

				unsigned char ucFileNameEnd;
				if (!bIsFolder && m_pJoliet->IncludesFileVerInfo())
					ucFileNameEnd = (ucNameLen >> 1) - 2;
				else
					ucFileNameEnd = (ucNameLen >> 1);

				if (CompareStrings(szFileName,(*itFile)->m_FileName.c_str(),ucFileNameEnd))
				{
#ifdef _UNICODE
					(*itFile)->m_FileNameJoliet = (*itFile)->m_FileName;

					if (!bIsFolder && m_pJoliet->IncludesFileVerInfo())
						(*itFile)->m_FileNameJoliet.append(L";1");
#else
					wchar_t szWideFileName[(ISO9660WRITER_FILENAME_BUFFER_SIZE >> 1) + 1];
					unsigned char ucFileNamePos = 0;
					for (unsigned char i = 0; i < (ucNameLen >> 1); i++)
					{
						szWideFileName[i]  = szFileName[ucFileNamePos++] << 8;
						szWideFileName[i] |= szFileName[ucFileNamePos++];
					}

					szWideFileName[ucNameLen >> 1] = '\0';

					(*itFile)->m_FileNameJoliet = szWideFileName;
#endif
				}
			}
			else
			{
				ucNameLen = m_pIso9660->WriteFileName(szFileName,(*itFile)->m_FileName.c_str(),bIsFolder);

				unsigned char ucFileNameEnd;
				if (!bIsFolder && m_pIso9660->IncludesFileVerInfo())
					ucFileNameEnd = ucNameLen - 2;
				else
					ucFileNameEnd = ucNameLen;

				if (CompareStrings((const char *)szFileName,(*itFile)->m_FileName.c_str(),ucFileNameEnd))
				{
					szFileName[ucNameLen] = '\0';
					(*itFile)->m_FileNameIso9660 = (const char *)szFileName;
				}
			}

			unsigned char ucCurRecSize = sizeof(tDirRecord) + ucNameLen - 1;

			// If the record length is not even padd it with a 0 byte.
			if (ucCurRecSize % 2 == 1)
				ucCurRecSize++;

			if (ulDirSecData + (ucCurRecSize * ulFactor) > ISO9660_SECTOR_SIZE)
			{
				ulDirSecData = ucCurRecSize * ulFactor;
				ulDirLength++;
			}
			else
			{
				ulDirSecData += ucCurRecSize * ulFactor;
			}
		}

		if (ulDirSecData != 0)
			ulDirLength++;

		return true;
	}

	bool Iso9660Writer::CalcLocalDirEntriesLength(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
		FileTreeNode *pLocalNode,int iLevel,ckcore::tuint64 &uiSecOffset,ckcore::Progress &Progress)
	{
		unsigned long ulDirLenNormal = 0;
		if (!CalcLocalDirEntryLength(pLocalNode,false,iLevel,ulDirLenNormal))
			return false;

		unsigned long ulDirLenJoliet = 0;
		if (m_bUseJoliet && !CalcLocalDirEntryLength(pLocalNode,true,iLevel,ulDirLenJoliet))
			return false;

		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (iLevel >= m_pIso9660->GetMaxDirLevel())
					continue;
				else
					DirNodeStack.push_back(std::make_pair(*itFile,iLevel + 1));
			}
		}

		// We now know how much space is needed to store the current directory record.
		pLocalNode->m_uiDataSizeNormal = ulDirLenNormal * ISO9660_SECTOR_SIZE;
		pLocalNode->m_uiDataSizeJoliet = ulDirLenJoliet * ISO9660_SECTOR_SIZE;

		pLocalNode->m_uiDataPosNormal = uiSecOffset;
		uiSecOffset += ulDirLenNormal;
		pLocalNode->m_uiDataPosJoliet = uiSecOffset;
		uiSecOffset += ulDirLenJoliet;

		return true;
	}

	bool Iso9660Writer::CalcDirEntriesLength(FileTree &file_tree,ckcore::Progress &Progress,
		ckcore::tuint64 uiStartSector,ckcore::tuint64 &uiLength)
	{
		FileTreeNode *pCurNode = file_tree.GetRoot();
		ckcore::tuint64 uiSecOffset = uiStartSector;

		std::vector<std::pair<FileTreeNode *,int> > DirNodeStack;
		if (!CalcLocalDirEntriesLength(DirNodeStack,pCurNode,0,uiSecOffset,Progress))
			return false;

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1].first;
			int iLevel = DirNodeStack[DirNodeStack.size() - 1].second;
			DirNodeStack.pop_back();

			if (!CalcLocalDirEntriesLength(DirNodeStack,pCurNode,iLevel,uiSecOffset,Progress))
				return false;
		}

		uiLength = uiSecOffset - uiStartSector;
		return true;
	}

	bool Iso9660Writer::WritePathTable(FileSet &Files,FileTree &file_tree,
		bool bJolietTable,bool bMSBF,ckcore::Progress &Progress)
	{
		FileTreeNode *pCurNode = file_tree.GetRoot();

		tPathTableRecord RootRecord,PathRecord;
		memset(&RootRecord,0,sizeof(RootRecord));

		RootRecord.ucDirIdentifierLen = 1;					// Always 1 for the root record.
		RootRecord.ucExtAttrRecordLen = 0;

		if (bJolietTable)
			Write73(RootRecord.ucExtentLocation,(unsigned long)pCurNode->m_uiDataPosJoliet,bMSBF);	// Start sector of extent data.
		else
			Write73(RootRecord.ucExtentLocation,(unsigned long)pCurNode->m_uiDataPosNormal,bMSBF);	// Start sector of extent data.

		Write72(RootRecord.ucParentDirNumber,0x01,bMSBF);	// The root has itself as parent.
		RootRecord.ucDirIdentifier[0] = 0;					// The file name is set to zero.

		// Write the root record.
		ckcore::tint64 iProcessed = m_pOutStream->Write(&RootRecord,sizeof(RootRecord));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(RootRecord))
			return false;

		// We need to pad the root record since it's size is otherwise odd.
		iProcessed = m_pOutStream->Write(RootRecord.ucDirIdentifier,1);
		if (iProcessed == -1)
			return false;
		if (iProcessed != 1)
			return false;

		// Write all other path table records.
		std::map<ckcore::tstring,unsigned short> PathDirNumMap;
		ckcore::tstring PathBuffer,CurDirName,InternalPath;

		// Counters for all records.
		unsigned short usRecordNumber = 2;	// Root has number 1.

		FileSet::const_iterator itFile;
		for (itFile = Files.begin(); itFile != Files.end(); itFile++)
		{
			// We're do not have to add the root record once again.
			int iLevel = FileComparator::Level(*itFile);
			if (iLevel <= 1)
				continue;

			if (iLevel > m_pIso9660->GetMaxDirLevel())
				continue;

			// We're only interested in directories.
			if (!(itFile->m_ucFlags & FileDescriptor::FLAG_DIRECTORY))
				continue;

			// Locate the node in the file tree.
			pCurNode = file_tree.GetNodeFromPath(*itFile);
			if (pCurNode == NULL)
				return false;

			// Make sure that the path to the current file or folder exists.
			size_t iPrevDelim = 0;

			// If the file is a directory, append a slash so it will be parsed as a
			// directory in the loop below.
			InternalPath = itFile->m_InternalPath;
			InternalPath.push_back('/');

			PathBuffer.erase();
			PathBuffer = ckT("/");

			unsigned short usParent = 1;	// Start from root.

			for (size_t i = 0; i < InternalPath.length(); i++)
			{
				if (InternalPath[i] == '/')
				{
					if (i > (iPrevDelim + 1))
					{
						// Obtain the name of the current directory.
						CurDirName.erase();
						for (size_t j = iPrevDelim + 1; j < i; j++)
							CurDirName.push_back(InternalPath[j]);

						PathBuffer += CurDirName;
						PathBuffer += ckT("/");

						// The path does not exist, create it.
						if (PathDirNumMap.find(PathBuffer) == PathDirNumMap.end())
						{
							memset(&PathRecord,0,sizeof(PathRecord));

							unsigned char ucNameLen;
							unsigned char szFileName[ISO9660WRITER_FILENAME_BUFFER_SIZE];	// Large enough for both level 1, 2 and even Joliet.
							if (bJolietTable)
							{
								ucNameLen = m_pJoliet->WriteFileName(szFileName,CurDirName.c_str(),true) << 1;
								MakeUniqueJoliet(pCurNode,szFileName,ucNameLen);
							}
							else
							{
								ucNameLen = m_pIso9660->WriteFileName(szFileName,CurDirName.c_str(),true);
								MakeUniqueIso9660(pCurNode,szFileName,ucNameLen);
							}

							// If the record length is not even padd it with a 0 byte.
							bool bPadByte = false;
							if (ucNameLen % 2 == 1)
								bPadByte = true;

							PathRecord.ucDirIdentifierLen = ucNameLen;
							PathRecord.ucExtAttrRecordLen = 0;

							if (bJolietTable)
								Write73(PathRecord.ucExtentLocation,(unsigned long)pCurNode->m_uiDataPosJoliet,bMSBF);
							else
								Write73(PathRecord.ucExtentLocation,(unsigned long)pCurNode->m_uiDataPosNormal,bMSBF);

							Write72(PathRecord.ucParentDirNumber,usParent,bMSBF);
							PathRecord.ucDirIdentifier[0] = 0;

							PathDirNumMap[PathBuffer] = usParent = usRecordNumber++;

							// Write the record.
							iProcessed = m_pOutStream->Write(&PathRecord,sizeof(PathRecord) - 1);
							if (iProcessed == -1)
								return false;
							if (iProcessed != sizeof(PathRecord) - 1)
								return false;

							iProcessed = m_pOutStream->Write(szFileName,ucNameLen);
							if (iProcessed == -1)
								return false;
							if (iProcessed != ucNameLen)
								return false;

							// Pad if necessary.
							if (bPadByte)
							{
								char szTemp[1] = { 0 };
								iProcessed = m_pOutStream->Write(szTemp,1);
								if (iProcessed == -1)
									return false;
								if (iProcessed != 1)
									return false;
							}
						}
						else
						{
							usParent = PathDirNumMap[PathBuffer];
						}
					}

					iPrevDelim = i;
				}
			}
		}

		if (m_pOutStream->GetAllocated() != 0)
			m_pOutStream->PadSector();

		return true;
	}

	bool Iso9660Writer::WriteSysDirectory(FileTreeNode *pParent,eSysDirType Type,
		unsigned long ulDataPos,unsigned long ulDataSize)
	{
		tDirRecord DirRecord;
		memset(&DirRecord,0,sizeof(DirRecord));

		DirRecord.ucDirRecordLen = 0x22;
		Write733(DirRecord.ucExtentLocation,ulDataPos);
		Write733(DirRecord.ucDataLen,/*ISO9660_SECTOR_SIZE*/ulDataSize);
		MakeDateTime(m_ImageCreate,DirRecord.RecDateTime);
		DirRecord.ucFileFlags = DIRRECORD_FILEFLAG_DIRECTORY;
		Write723(DirRecord.ucVolSeqNumber,0x01);	// The directory is on the first volume set.
		DirRecord.ucFileIdentifierLen = 1;
		DirRecord.ucFileIdentifier[0] = Type == TYPE_CURRENT ? 0 : 1;

		ckcore::tint64 iProcessed = m_pOutStream->Write(&DirRecord,sizeof(DirRecord));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(DirRecord))
			return false;

		return true;
	}

	bool Iso9660Writer::ValidateTreeNode(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
		FileTreeNode *pNode,int iLevel)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pNode->m_Children.begin(); itFile !=
			pNode->m_Children.end(); itFile++)
		{
			if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
			{
				if (iLevel >= m_pIso9660->GetMaxDirLevel())
					return false;
				else
					DirNodeStack.push_back(std::make_pair(*itFile,iLevel + 1));
			}
		}

		return true;
	}

	/*
		Returns true if the specified file tree conforms to the configuration
		used to create the disc image.
	*/
	bool Iso9660Writer::ValidateTree(FileTree &file_tree)
	{
		FileTreeNode *pCurNode = file_tree.GetRoot();

		std::vector<std::pair<FileTreeNode *,int> > DirNodeStack;
		if (!ValidateTreeNode(DirNodeStack,pCurNode,1))
			return false;

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1].first;
			int iLevel = DirNodeStack[DirNodeStack.size() - 1].second;
			DirNodeStack.pop_back();

			if (!ValidateTreeNode(DirNodeStack,pCurNode,iLevel))
				return false;
		}

		return true;
	}

	int Iso9660Writer::AllocateHeader()
	{
		// Allocate volume descriptor.
		unsigned long ulVolDescSize = sizeof(tVolDescPrimary) + sizeof(tVolDescSetTerm);
		if (m_pElTorito->GetBootImageCount() > 0)
			ulVolDescSize += sizeof(tVolDescElToritoRecord);
		if (m_pIso9660->HasVolDescSuppl())
			ulVolDescSize += sizeof(tVolDescSuppl);
		if (m_bUseJoliet)
			ulVolDescSize += sizeof(tVolDescSuppl);

		m_pSectorManager->AllocateBytes(this,SR_DESCRIPTORS,ulVolDescSize);

		// Allocate boot catalog and data.
		if (m_pElTorito->GetBootImageCount() > 0)
		{
			m_pSectorManager->AllocateBytes(this,SR_BOOTCATALOG,m_pElTorito->GetBootCatSize());
			m_pSectorManager->AllocateBytes(this,SR_BOOTDATA,m_pElTorito->GetBootDataSize());
		}

		return RESULT_OK;
	}

	int Iso9660Writer::AllocatePathTables(ckcore::Progress &Progress,FileSet &Files)
	{
		// Calculate path table sizes.
		m_uiPathTableSizeNormal = 0;
		if (!CalcPathTableSize(Files,false,m_uiPathTableSizeNormal,Progress))
		{
			m_pLog->PrintLine(ckT("  Error: Unable to calculate path table size."));
			return RESULT_FAIL;
		}

		m_uiPathTableSizeJoliet = 0;
		if (m_bUseJoliet && !CalcPathTableSize(Files,true,m_uiPathTableSizeJoliet,Progress))
		{
			m_pLog->PrintLine(ckT("  Error: Unable to calculate Joliet path table size."));
			return RESULT_FAIL;
		}

		if (m_uiPathTableSizeNormal > 0xFFFFFFFF || m_uiPathTableSizeJoliet > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			m_pLog->PrintLine(ckT("  Error: The path table is too large, %I64u and %I64u bytes."),
#else
			m_pLog->PrintLine(ckT("  Error: The path table is too large, %llu and %llu bytes."),
#endif
				m_uiPathTableSizeNormal,m_uiPathTableSizeJoliet);
			Progress.Notify(ckcore::Progress::ckERROR,g_StringTable.GetString(ERROR_PATHTABLESIZE));
			return RESULT_FAIL;
		}

		m_pSectorManager->AllocateBytes(this,SR_PATHTABLE_NORMAL_L,m_uiPathTableSizeNormal);
		m_pSectorManager->AllocateBytes(this,SR_PATHTABLE_NORMAL_M,m_uiPathTableSizeNormal);
		m_pSectorManager->AllocateBytes(this,SR_PATHTABLE_JOLIET_L,m_uiPathTableSizeJoliet);
		m_pSectorManager->AllocateBytes(this,SR_PATHTABLE_JOLIET_M,m_uiPathTableSizeJoliet);

		return RESULT_OK;
	}

	int Iso9660Writer::AllocateDirEntries(FileTree &file_tree,ckcore::Progress &Progress)
	{
		ckcore::tuint64 uiDirEntriesLen = 0;
		if (!CalcDirEntriesLength(file_tree,Progress,m_pSectorManager->GetNextFree(),uiDirEntriesLen))
			return RESULT_FAIL;

		m_pSectorManager->AllocateSectors(this,SR_DIRENTRIES,uiDirEntriesLen);

#ifdef _WINDOWS
		m_pLog->PrintLine(ckT("  Allocated directory entries %I64u sectors."),uiDirEntriesLen);
#else
		m_pLog->PrintLine(ckT("  Allocated directory entries %llu sectors."),uiDirEntriesLen);
#endif
		return RESULT_OK;
	}

	int Iso9660Writer::WriteHeader(FileSet &Files,FileTree &file_tree,ckcore::Progress &Progress)
	{
		// Make sure that everything has been allocated.
		if (m_uiPathTableSizeNormal == 0)
		{
			m_pLog->PrintLine(ckT("  Error: Memory for ISO9660 path table has not been allocated."));
			return RESULT_FAIL;
		}

		// Calculate boot catalog and image data.
		if (m_pElTorito->GetBootImageCount() > 0)
		{
			ckcore::tuint64 uiBootDataSector = m_pSectorManager->GetStart(this,SR_BOOTDATA);
			ckcore::tuint64 uiBootDataLength = BytesToSector64(m_pElTorito->GetBootDataSize());
			ckcore::tuint64 uiBootDataEnd = uiBootDataSector + uiBootDataLength;

			if (uiBootDataSector > 0xFFFFFFFF || uiBootDataEnd > 0xFFFFFFFF)
			{
#ifdef _WINDOWS
				m_pLog->PrintLine(ckT("  Error: Invalid boot data sector range (%I64u to %I64u)."),
#else
				m_pLog->PrintLine(ckT("  Error: Invalid boot data sector range (%llu to %llu)."),
#endif
					uiBootDataSector,uiBootDataEnd);
				return RESULT_FAIL;
			}

			if (!m_pElTorito->CalculateFileSysData(uiBootDataSector,uiBootDataEnd))
				return RESULT_FAIL;
		}

		unsigned long ulPosPathTableNormalL = (unsigned long)m_pSectorManager->GetStart(this,SR_PATHTABLE_NORMAL_L);
		unsigned long ulPosPathTableNormalM = (unsigned long)m_pSectorManager->GetStart(this,SR_PATHTABLE_NORMAL_M);
		unsigned long ulPosPathTableJolietL = (unsigned long)m_pSectorManager->GetStart(this,SR_PATHTABLE_JOLIET_L);
		unsigned long ulPosPathTableJolietM = (unsigned long)m_pSectorManager->GetStart(this,SR_PATHTABLE_JOLIET_M);

		// Print the sizes.
		ckcore::tuint64 uiDirEntriesSector = m_pSectorManager->GetStart(this,SR_DIRENTRIES);
		ckcore::tuint64 uiFileDataEndSector = m_pSectorManager->GetDataStart() + m_pSectorManager->GetDataLength();

		if (uiDirEntriesSector > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			m_pLog->PrintLine(ckT("  Error: Invalid start sector of directory entries (%I64u)."),uiDirEntriesSector);
#else
			m_pLog->PrintLine(ckT("  Error: Invalid start sector of directory entries (%llu)."),uiDirEntriesSector);
#endif
			return RESULT_FAIL;
		}

		if (!m_pIso9660->WriteVolDescPrimary(m_pOutStream,m_ImageCreate,(unsigned long)uiFileDataEndSector,
				(unsigned long)m_uiPathTableSizeNormal,ulPosPathTableNormalL,ulPosPathTableNormalM,
				(unsigned long)uiDirEntriesSector,(unsigned long)file_tree.GetRoot()->m_uiDataSizeNormal))
		{
			return RESULT_FAIL;
		}

		// Write the El Torito boot record at sector 17 if necessary.
		if (m_pElTorito->GetBootImageCount() > 0)
		{
			ckcore::tuint64 uiBootCatSector = m_pSectorManager->GetStart(this,SR_BOOTCATALOG);
			if (!m_pElTorito->WriteBootRecord(m_pOutStream,(unsigned long)uiBootCatSector))
			{
#ifdef _WINDOWS
				m_pLog->PrintLine(ckT("  Error: Failed to write boot record at sector %I64u."),uiBootCatSector);
#else
				m_pLog->PrintLine(ckT("  Error: Failed to write boot record at sector %llu."),uiBootCatSector);
#endif
				return RESULT_FAIL;
			}

			m_pLog->PrintLine(ckT("  Wrote El Torito boot record at sector %d."),uiBootCatSector);
		}

		// Write ISO9660 descriptor.
		if (m_pIso9660->HasVolDescSuppl())
		{
			if (!m_pIso9660->WriteVolDescSuppl(m_pOutStream,m_ImageCreate,(unsigned long)uiFileDataEndSector,
				(unsigned long)m_uiPathTableSizeNormal,ulPosPathTableNormalL,ulPosPathTableNormalM,
				(unsigned long)uiDirEntriesSector,(unsigned long)file_tree.GetRoot()->m_uiDataSizeNormal))
			{
				m_pLog->PrintLine(ckT("  Error: Failed to write supplementary volume descriptor."));
				return RESULT_FAIL;
			}
		}

		// Write the Joliet header.
		if (m_bUseJoliet)
		{
			if (file_tree.GetRoot()->m_uiDataSizeNormal > 0xFFFFFFFF)
			{
#ifdef _WINDOWS
				m_pLog->PrintLine(ckT("  Error: Root folder is larger (%I64u) than the ISO9660 file system can handle."),
#else
				m_pLog->PrintLine(ckT("  Error: Root folder is larger (%llu) than the ISO9660 file system can handle."),
#endif
				file_tree.GetRoot()->m_uiDataSizeNormal);
				return RESULT_FAIL;
			}

			unsigned long ulRootExtentLocJoliet = (unsigned long)uiDirEntriesSector +
				BytesToSector((unsigned long)file_tree.GetRoot()->m_uiDataSizeNormal);

			if (!m_pJoliet->WriteVolDesc(m_pOutStream,m_ImageCreate,(unsigned long)uiFileDataEndSector,
				(unsigned long)m_uiPathTableSizeJoliet,ulPosPathTableJolietL,ulPosPathTableJolietM,
				ulRootExtentLocJoliet,(unsigned long)file_tree.GetRoot()->m_uiDataSizeJoliet))
			{
				return false;
			}
		}

		if (!m_pIso9660->WriteVolDescSetTerm(m_pOutStream))
			return false;

		// Write the El Torito boot catalog and boot image data.
		if (m_pElTorito->GetBootImageCount() > 0)
		{
			if (!m_pElTorito->WriteBootCatalog(m_pOutStream))
			{
				m_pLog->PrintLine(ckT("  Error: Failed to write boot catalog."));
				return false;
			}

			if (!m_pElTorito->WriteBootImages(m_pOutStream))
			{
				m_pLog->PrintLine(ckT("  Error: Failed to write images."));
				return false;
			}
		}

		return RESULT_OK;
	}

	int Iso9660Writer::WritePathTables(FileSet &Files,FileTree &file_tree,ckcore::Progress &Progress)
	{
		Progress.SetStatus(g_StringTable.GetString(STATUS_WRITEISOTABLE));

		// Write the path tables.
		if (!WritePathTable(Files,file_tree,false,false,Progress))
		{
			m_pLog->PrintLine(ckT("  Error: Failed to write path table (LSBF)."));
			return RESULT_FAIL;
		}
		if (!WritePathTable(Files,file_tree,false,true,Progress))
		{
			m_pLog->PrintLine(ckT("  Error: Failed to write path table (MSBF)."));
			return RESULT_FAIL;
		}

		if (m_bUseJoliet)
		{
			Progress.SetStatus(g_StringTable.GetString(STATUS_WRITEJOLIETTABLE));

			if (!WritePathTable(Files,file_tree,true,false,Progress))
			{
				m_pLog->PrintLine(ckT("  Error: Failed to write Joliet path table (LSBF)."));
				return RESULT_FAIL;
			}
			if (!WritePathTable(Files,file_tree,true,true,Progress))
			{
				m_pLog->PrintLine(ckT("  Error: Failed to write Joliet path table (MSBF)."));
				return RESULT_FAIL;
			}
		}

		return RESULT_OK;
	}

	int Iso9660Writer::WriteLocalDirEntry(ckcore::Progress &Progress,FileTreeNode *pLocalNode,
		bool bJoliet,int iLevel)
	{
		tDirRecord DirRecord;

		// Write the '.' and '..' directories.
		FileTreeNode *pParentNode = pLocalNode->GetParent();
		if (pParentNode == NULL)
			pParentNode = pLocalNode;

		if (bJoliet)
		{
			WriteSysDirectory(pLocalNode,TYPE_CURRENT,
				(unsigned long)pLocalNode->m_uiDataPosJoliet,
				(unsigned long)pLocalNode->m_uiDataSizeJoliet);
			WriteSysDirectory(pLocalNode,TYPE_PARENT,
				(unsigned long)pParentNode->m_uiDataPosJoliet,
				(unsigned long)pParentNode->m_uiDataSizeJoliet);
		}
		else
		{
			WriteSysDirectory(pLocalNode,TYPE_CURRENT,
				(unsigned long)pLocalNode->m_uiDataPosNormal,
				(unsigned long)pLocalNode->m_uiDataSizeNormal);
			WriteSysDirectory(pLocalNode,TYPE_PARENT,
				(unsigned long)pParentNode->m_uiDataPosNormal,
				(unsigned long)pParentNode->m_uiDataSizeNormal);
		}

		// The number of bytes of data in the current sector.
		// Each directory always includes '.' and '..'.
		unsigned long ulDirSecData = sizeof(tDirRecord) << 1;

		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			// Check if we should abort.
			if (Progress.Cancelled())
				return RESULT_CANCEL;

			if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (iLevel >= m_pIso9660->GetMaxDirLevel())
					continue;
			}

			// Validate file size.
			if ((*itFile)->m_uiFileSize > ISO9660_MAX_EXTENT_SIZE && !m_pIso9660->AllowsFragmentation())
				continue;

			// This loop is necessary for multi-extent support.
			ckcore::tuint64 uiFileRemain = bJoliet ? (*itFile)->m_uiDataSizeJoliet : (*itFile)->m_uiDataSizeNormal;
			ckcore::tuint64 uiExtentLoc = bJoliet ? (*itFile)->m_uiDataPosJoliet : (*itFile)->m_uiDataPosNormal;

			do
			{
				// We can't actually use 0xFFFFFFFF bytes since that will not fit perfectly withing a sector range.
				unsigned long ulExtentSize = uiFileRemain > ISO9660_MAX_EXTENT_SIZE ?
					ISO9660_MAX_EXTENT_SIZE : (unsigned long)uiFileRemain;

				memset(&DirRecord,0,sizeof(DirRecord));

				// Make a valid file name.
				unsigned char ucNameLen;	// FIXME: Rename to ucNameSize;
				unsigned char szFileName[ISO9660WRITER_FILENAME_BUFFER_SIZE + 4]; // Large enough for level 1, 2 and even Joliet.

				bool bIsFolder = (*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY;
				if (bJoliet)
				{
					ucNameLen = m_pJoliet->WriteFileName(szFileName,(*itFile)->m_FileName.c_str(),bIsFolder) << 1;
					MakeUniqueJoliet((*itFile),szFileName,ucNameLen);
				}
				else
				{
					ucNameLen = m_pIso9660->WriteFileName(szFileName,(*itFile)->m_FileName.c_str(),bIsFolder);
					MakeUniqueIso9660((*itFile),szFileName,ucNameLen);
				}

				// If the record length is not even padd it with a 0 byte.
				bool bPadByte = false;
				unsigned char ucDirRecSize = sizeof(DirRecord) + ucNameLen - 1;
				if (ucDirRecSize % 2 == 1)
				{
					bPadByte = true;
					ucDirRecSize++;
				}

				DirRecord.ucDirRecordLen = ucDirRecSize;	// FIXME: Rename member.
				DirRecord.ucExtAttrRecordLen = 0;

				Write733(DirRecord.ucExtentLocation,(unsigned long)uiExtentLoc);
				Write733(DirRecord.ucDataLen,(unsigned long)ulExtentSize);

				if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_IMPORTED)
				{
					Iso9660ImportData *pImportNode = (Iso9660ImportData *)(*itFile)->m_pData;
					if (pImportNode == NULL)
					{
						m_pLog->PrintLine(ckT("  Error: The file \"%s\" does not contain imported session data like advertised."),
							(*itFile)->m_FileName.c_str());
						return RESULT_FAIL;
					}

					memcpy(&DirRecord.RecDateTime,&pImportNode->m_RecDateTime,sizeof(tDirRecordDateTime));

					DirRecord.ucFileFlags = pImportNode->m_ucFileFlags;
					DirRecord.ucFileUnitSize = pImportNode->m_ucFileUnitSize;
					DirRecord.ucInterleaveGapSize = pImportNode->m_ucInterleaveGapSize;
					Write723(DirRecord.ucVolSeqNumber,pImportNode->m_usVolSeqNumber);
				}
				else
				{
					// Date time.
					if (m_bUseFileTimes)
					{
						struct tm AccessTime,ModifyTime,CreateTime;
						bool bResult = true;

						if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
						{
							bResult = ckcore::Directory::Time((*itFile)->m_FileFullPath.c_str(),
															  AccessTime,ModifyTime,CreateTime);
							
						}
						else
						{
							bResult = ckcore::File::Time((*itFile)->m_FileFullPath.c_str(),
														 AccessTime,ModifyTime,CreateTime);
						}

						unsigned short usFileDate = 0,usFileTime = 0;
						ckcore::convert::tm_to_dostime(ModifyTime,usFileDate,usFileTime);

						if (bResult)
							MakeDateTime(usFileDate,usFileTime,DirRecord.RecDateTime);
						else
							MakeDateTime(m_ImageCreate,DirRecord.RecDateTime);
					}
					else
					{
						// The time when the disc image creation was initialized.
						MakeDateTime(m_ImageCreate,DirRecord.RecDateTime);
					}

					// File flags.
					DirRecord.ucFileFlags = 0;
					if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
						DirRecord.ucFileFlags |= DIRRECORD_FILEFLAG_DIRECTORY;

					if (ckcore::File::Hidden((*itFile)->m_FileFullPath.c_str()))
						DirRecord.ucFileFlags |= DIRRECORD_FILEFLAG_HIDDEN;

					DirRecord.ucFileUnitSize = 0;
					DirRecord.ucInterleaveGapSize = 0;
					Write723(DirRecord.ucVolSeqNumber,0x01);
				}

				// Remaining bytes, before checking if we're dealing with the last segment.
				uiFileRemain -= ulExtentSize;
				if ((*itFile)->m_uiFileSize > ISO9660_MAX_EXTENT_SIZE && uiFileRemain > 0)
					DirRecord.ucFileFlags |= DIRRECORD_FILEFLAG_MULTIEXTENT;

				DirRecord.ucFileIdentifierLen = ucNameLen;

				// Pad the sector with zeros if we can not fit the complete
				// directory entry on this sector.
				if ((ulDirSecData + ucDirRecSize) > ISO9660_SECTOR_SIZE)
				{
					ulDirSecData = ucDirRecSize;
					
					// Pad the sector with zeros.
					m_pOutStream->PadSector();
				}
				else if ((ulDirSecData + ucDirRecSize) == ISO9660_SECTOR_SIZE)
				{
					ulDirSecData = 0;
				}
				else
				{
					ulDirSecData += ucDirRecSize;
				}

				// Write the record.
				ckcore::tint64 iProcessed = m_pOutStream->Write(&DirRecord,sizeof(DirRecord) - 1);
				if (iProcessed == -1)
					return RESULT_FAIL;
				if (iProcessed != sizeof(DirRecord) - 1)
					return RESULT_FAIL;

				iProcessed = m_pOutStream->Write(szFileName,ucNameLen);
				if (iProcessed == -1)
					return RESULT_FAIL;
				if (iProcessed != ucNameLen)
					return RESULT_FAIL;

				// Pad if necessary.
				if (bPadByte)
				{
					char szTemp[1] = { 0 };
					iProcessed = m_pOutStream->Write(szTemp,1);
					if (iProcessed == -1)
						return RESULT_FAIL;
					if (iProcessed != 1)
						return RESULT_FAIL;
				}

				// Update location of the next extent.
				uiExtentLoc += BytesToSector(ulExtentSize);
			}
			while (uiFileRemain > 0);
		}

		if (m_pOutStream->GetAllocated() != 0)
			m_pOutStream->PadSector();

		return RESULT_OK;
	}

	int Iso9660Writer::WriteLocalDirEntries(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
		ckcore::Progress &Progress,FileTreeNode *pLocalNode,int iLevel)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (iLevel >= m_pIso9660->GetMaxDirLevel())
					continue;
				else
					DirNodeStack.push_back(std::make_pair(*itFile,iLevel + 1));
			}
		}

		int iResult = WriteLocalDirEntry(Progress,pLocalNode,false,iLevel);
		if (iResult != RESULT_OK)
			return iResult;

		if (m_bUseJoliet)
		{
			iResult = WriteLocalDirEntry(Progress,pLocalNode,true,iLevel);
			if (iResult != RESULT_OK)
				return iResult;
		}

		return RESULT_OK;
	}

	int Iso9660Writer::WriteDirEntries(FileTree &file_tree,ckcore::Progress &Progress)
	{
		Progress.SetStatus(g_StringTable.GetString(STATUS_WRITEDIRENTRIES));

		FileTreeNode *pCurNode = file_tree.GetRoot();

		std::vector<std::pair<FileTreeNode *,int> > DirNodeStack;
		if (!WriteLocalDirEntries(DirNodeStack,Progress,pCurNode,1))
			return RESULT_FAIL;

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1].first;
			int iLevel = DirNodeStack[DirNodeStack.size() - 1].second;
			DirNodeStack.pop_back();

			int iResult = WriteLocalDirEntries(DirNodeStack,Progress,pCurNode,iLevel);
			if (iResult != RESULT_OK)
				return iResult;
		}

		return RESULT_OK;
	}
};
