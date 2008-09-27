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

#include "ckfilesystem/filetree.hh"

namespace ckfilesystem
{
	FileTree::FileTree(ckcore::Log &log) :
		log_(log),m_pRootNode(NULL)
	{
		m_ulDirCount = 0;
		m_ulFileCount = 0;
	}

	FileTree::~FileTree()
	{
		if (m_pRootNode != NULL)
		{
			delete m_pRootNode;
			m_pRootNode = NULL;
		}
	}

	FileTreeNode *FileTree::GetRoot()
	{
		return m_pRootNode;
	}

	FileTreeNode *FileTree::GetChildFromFileName(FileTreeNode *pParent,const ckcore::tchar *szFileName)
	{
		std::vector<FileTreeNode *>::const_iterator itChild;
		for (itChild = pParent->m_Children.begin(); itChild !=
			pParent->m_Children.end(); itChild++)
		{
			if (!ckcore::string::astrcmp(szFileName,(*itChild)->m_FileName.c_str()))
				return *itChild;
		}

		return NULL;
	}

	bool FileTree::AddFileFromPath(const FileDescriptor &File)
	{
		size_t iDirPathLen = File.m_InternalPath.length(),iPrevDelim = 0,iPos;
		ckcore::tstring CurDirName;
		FileTreeNode *pCurNode = m_pRootNode;

		for (iPos = 0; iPos < iDirPathLen; iPos++)
		{
			if (File.m_InternalPath.c_str()[iPos] == '/')
			{
				if (iPos > (iPrevDelim + 1))
				{
					// Obtain the name of the current directory.
					CurDirName.erase();
					for (size_t j = iPrevDelim + 1; j < iPos; j++)
						CurDirName.push_back(File.m_InternalPath.c_str()[j]);

					pCurNode = GetChildFromFileName(pCurNode,CurDirName.c_str());
					if (pCurNode == NULL)
					{
						log_.PrintLine(ckT("  Error: Unable to find child node \"%s\" in path \"%s\"."),
							CurDirName.c_str(),File.m_InternalPath.c_str());
						return false;
					}
				}

				iPrevDelim = iPos;
			}
		}

		// We now have our parent.
		const ckcore::tchar *szFileName = File.m_InternalPath.c_str() + iPrevDelim + 1;

		// Check if imported.
		unsigned char ucImportFlag = 0;
		void *pImportData = NULL;
		if (File.m_ucFlags & FileDescriptor::FLAG_IMPORTED)
		{
			ucImportFlag = FileTreeNode::FLAG_IMPORTED;
			pImportData = File.m_pData;
		}

		if (File.m_ucFlags & FileDescriptor::FLAG_DIRECTORY)
		{
			pCurNode->m_Children.push_back(new FileTreeNode(pCurNode,szFileName,
				File.m_ExternalPath.c_str(),0,true,0,FileTreeNode::FLAG_DIRECTORY | ucImportFlag,
				pImportData));

			m_ulDirCount++;
		}
		else
		{
			pCurNode->m_Children.push_back(new FileTreeNode(pCurNode,szFileName,
				File.m_ExternalPath.c_str(),File.m_uiFileSize,true,0,ucImportFlag,pImportData));

			m_ulFileCount++;
		}

		return true;
	}

	bool FileTree::CreateFromFileSet(const FileSet &Files)
	{
		if (m_pRootNode != NULL)
			delete m_pRootNode;

		m_pRootNode = new FileTreeNode(NULL,ckT(""),ckT(""),0,true,0,
			FileTreeNode::FLAG_DIRECTORY);

		FileSet::const_iterator itFile;
		for (itFile = Files.begin(); itFile != Files.end(); itFile++)
		{
			if (!AddFileFromPath(*itFile))
				return false;
		}

		return true;
	}

	FileTreeNode *FileTree::GetNodeFromPath(const FileDescriptor &File)
	{
		//m_pLog->PrintLine(ckT("BEGIN: %s"),File.m_ExternalPath.c_str());
		size_t iDirPathLen = File.m_InternalPath.length(),iPrevDelim = 0,iPos;
		ckcore::tstring CurDirName;
		FileTreeNode *pCurNode = m_pRootNode;

		for (iPos = 0; iPos < iDirPathLen; iPos++)
		{
			if (File.m_InternalPath.c_str()[iPos] == '/')
			{
				if (iPos > (iPrevDelim + 1))
				{
					// Obtain the name of the current directory.
					CurDirName.erase();
					for (size_t j = iPrevDelim + 1; j < iPos; j++)
						CurDirName.push_back(File.m_InternalPath.c_str()[j]);

					pCurNode = GetChildFromFileName(pCurNode,CurDirName.c_str());
					if (pCurNode == NULL)
					{
						log_.PrintLine(ckT("  Error: Unable to find child node \"%s\"."),CurDirName.c_str());
						return NULL;
					}
				}

				iPrevDelim = iPos;
			}
		}

		// We now have our parent.
		const ckcore::tchar *szFileName = File.m_InternalPath.c_str() + iPrevDelim + 1;

		//m_pLog->PrintLine(ckT("  END: %s"),File.m_ExternalPath.c_str());
		return GetChildFromFileName(pCurNode,szFileName);
	}

	FileTreeNode *FileTree::GetNodeFromPath(const ckcore::tchar *szInternalPath)
	{
		size_t iDirPathLen = ckcore::string::astrlen(szInternalPath),iPrevDelim = 0,iPos;
		ckcore::tstring CurDirName;
		FileTreeNode *pCurNode = m_pRootNode;

		for (iPos = 0; iPos < iDirPathLen; iPos++)
		{
			if (szInternalPath[iPos] == '/')
			{
				if (iPos > (iPrevDelim + 1))
				{
					// Obtain the name of the current directory.
					CurDirName.erase();
					for (size_t j = iPrevDelim + 1; j < iPos; j++)
						CurDirName.push_back(szInternalPath[j]);

					pCurNode = GetChildFromFileName(pCurNode,CurDirName.c_str());
					if (pCurNode == NULL)
					{
						log_.PrintLine(ckT("  Error: Unable to find child node \"%s\"."),CurDirName.c_str());
						return NULL;
					}
				}

				iPrevDelim = iPos;
			}
		}

		// We now have our parent.
		const ckcore::tchar *szFileName = szInternalPath + iPrevDelim + 1;

		return GetChildFromFileName(pCurNode,szFileName);
	}

	/**
		@eturn the number of files in the tree, fragmented files are counted once.
	*/
	unsigned long FileTree::GetDirCount()
	{
		return m_ulDirCount;
	}

	/**
		@return the number of directories in the tree, the root is not included.
	*/
	unsigned long FileTree::GetFileCount()
	{
		return m_ulFileCount;
	}

#ifdef _DEBUG
	void FileTree::PrintLocalTree(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
								   FileTreeNode *pLocalNode,int iIndent)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			if ((*itFile)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				DirNodeStack.push_back(std::make_pair(*itFile,iIndent));
			}
			else
			{
				for (int i = 0; i < iIndent; i++)
					log_.Print(ckT(" "));

				log_.Print(ckT("<f>"));
				log_.Print((*itFile)->m_FileName.c_str());
#ifdef _WINDOWS
				log_.PrintLine(ckT(" (%I64u:%I64u,%I64u:%I64u,%I64u:%I64u)"),(*itFile)->m_uiDataPosNormal,
#else
				log_.PrintLine(ckT(" (%llu:%llu,%llu:%llu,%llu:%llu)"),(*itFile)->m_uiDataPosNormal,
#endif
					(*itFile)->m_uiDataSizeNormal,(*itFile)->m_uiDataPosJoliet,
					(*itFile)->m_uiDataSizeJoliet,(*itFile)->m_uiUdfSize,(*itFile)->m_uiUdfSizeTot);
			}
		}
	}

	void FileTree::PrintTree()
	{
		if (m_pRootNode == NULL)
			return;

		FileTreeNode *pCurNode = m_pRootNode;
		int iIndent = 0;

		log_.PrintLine(ckT("FileTree::PrintTree"));
#ifdef _WINDOWS
		log_.PrintLine(ckT("  <root> (%I64u:%I64u,%I64u:%I64u,%I64u:%I64u)"),pCurNode->m_uiDataPosNormal,
#else
		log_.PrintLine(ckT("  <root> (%llu:%llu,%llu:%llu,%llu:%llu)"),pCurNode->m_uiDataPosNormal,
#endif
		pCurNode->m_uiDataSizeNormal,pCurNode->m_uiDataPosJoliet,
		pCurNode->m_uiDataSizeJoliet,pCurNode->m_uiUdfSize,pCurNode->m_uiUdfSizeTot);

		std::vector<std::pair<FileTreeNode *,int> > DirNodeStack;
		PrintLocalTree(DirNodeStack,pCurNode,4);

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1].first;
			iIndent = DirNodeStack[DirNodeStack.size() - 1].second;

			DirNodeStack.pop_back();

			// Print the directory name.
			for (int i = 0; i < iIndent; i++)
				log_.Print(ckT(" "));

			log_.Print(ckT("<d>"));
			log_.Print(pCurNode->m_FileName.c_str());
#ifdef _WINDOWS
			log_.PrintLine(ckT(" (%I64u:%I64u,%I64u:%I64u,%I64u:%I64u)"),pCurNode->m_uiDataPosNormal,
#else
			log_.PrintLine(ckT(" (%llu:%llu,%llu:%llu,%llu:%llu)"),pCurNode->m_uiDataPosNormal,
#endif
				pCurNode->m_uiDataSizeNormal,pCurNode->m_uiDataPosJoliet,
				pCurNode->m_uiDataSizeJoliet,pCurNode->m_uiUdfSize,pCurNode->m_uiUdfSizeTot);

			PrintLocalTree(DirNodeStack,pCurNode,iIndent + 2);
		}
	}
#endif
};
