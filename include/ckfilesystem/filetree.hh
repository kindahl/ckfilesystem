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
#include <vector>
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include "ckfilesystem/fileset.hh"

namespace ckfilesystem
{
	class FileTreeNode
	{
	private:
		FileTreeNode *m_pParent;

	public:
		std::vector<FileTreeNode *> m_Children;

		// File information.
		enum
		{
			FLAG_DIRECTORY = 0x01,
			FLAG_IMPORTED = 0x02
		};

		unsigned char m_ucFileFlags;
		ckcore::tuint64 m_uiFileSize;
		ckcore::tstring m_FileName;			// File name in disc image (requested name not actual, using ISO9660 may cripple the name).
		ckcore::tstring m_FileFullPath;		// Place on hard drive.

		// I am not sure this is the best way, this uses lots of memory.
		std::string m_FileNameIso9660;
		std::wstring m_FileNameJoliet;

		// File system information (not set by the routines in this file).
		ckcore::tuint64 m_uiDataPosNormal;	// Sector number of first sector containing data.
		ckcore::tuint64 m_uiDataPosJoliet;
		ckcore::tuint64 m_uiDataSizeNormal;	// Data length in bytes.
		ckcore::tuint64 m_uiDataSizeJoliet;

		unsigned long m_ulDataPadLen;		// The number of sectors to pad with zeroes after the file.

		// Sector size of UDF partition entry (all data) for an node and all it's children.
		ckcore::tuint64 m_uiUdfSize;
		ckcore::tuint64 m_uiUdfSizeTot;
		ckcore::tuint64 m_uiUdfLinkTot;		// The number of directory links within the UDF file system.
		unsigned long m_ulUdfPartLoc;		// Where is the actual UDF file entry stored.

		void *m_pData;						// Pointer to a user-defined structure, designed for CIso9660TreeNode

		FileTreeNode(FileTreeNode *pParent,const ckcore::tchar *szFileName,
			const ckcore::tchar *szFileFullPath,ckcore::tuint64 uiFileSize,
			bool bLastFragment,unsigned long ulFragmentIndex,
			unsigned char ucFileFlags = 0,void *pData = NULL)
		{
			m_pParent = pParent;

			m_ucFileFlags = ucFileFlags;
			m_uiFileSize = uiFileSize;
			m_FileName = szFileName;
			m_FileFullPath = szFileFullPath;

			m_uiDataPosNormal = 0;
			m_uiDataPosJoliet = 0;
			m_uiDataSizeNormal = 0;
			m_uiDataSizeJoliet = 0;
			m_ulDataPadLen = 0;

			m_uiUdfSize = 0;
			m_uiUdfSizeTot = 0;
			m_uiUdfLinkTot = 0;
			m_ulUdfPartLoc = 0;

			m_pData = pData;
		}

		~FileTreeNode()
		{
			// Free the children.
			std::vector<FileTreeNode *>::iterator itNode;
			for (itNode = m_Children.begin(); itNode != m_Children.end(); itNode++)
				delete *itNode;

			m_Children.clear();
		}

		FileTreeNode *GetParent()
		{
			return m_pParent;
		}
	};

	class FileTree
	{
	private:
		ckcore::Log *m_pLog;
		FileTreeNode *m_pRootNode;

		// File tree information.
		unsigned long m_ulDirCount;
		unsigned long m_ulFileCount;

		FileTreeNode *GetChildFromFileName(FileTreeNode *pParent,const ckcore::tchar *szFileName);
		bool AddFileFromPath(const FileDescriptor &File);

	public:
		FileTree(ckcore::Log *pLog);
		~FileTree();

		FileTreeNode *GetRoot();
		
		bool CreateFromFileSet(const FileSet &Files);
		FileTreeNode *GetNodeFromPath(const FileDescriptor &File);
		FileTreeNode *GetNodeFromPath(const ckcore::tchar *szInternalPath);

	#ifdef _DEBUG
		void PrintLocalTree(std::vector<std::pair<FileTreeNode *,int> > &DirNodeStack,
			FileTreeNode *pLocalNode,int iIndent);
		void PrintTree();
	#endif

		// For obtaining file tree information.
		unsigned long GetDirCount();
		unsigned long GetFileCount();
	};
};
