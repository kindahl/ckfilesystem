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
#include <ckcore/stream.hh>
#include "ckfilesystem/iso9660.hh"

namespace ckfilesystem
{
	// A Iso9660TreeNode tree node contains every information needed to write
	// an ISO9660 directory record.
	class Iso9660TreeNode
	{
	private:
		Iso9660TreeNode *m_pParent;

	public:
		std::vector<Iso9660TreeNode *> m_Children;

		unsigned char m_ucFileFlags;
		unsigned char m_ucFileUnitSize;
		unsigned char m_ucInterleaveGapSize;
		unsigned short m_usVolSeqNumber;
		unsigned long m_ulExtentLocation;
		unsigned long m_ulExtentLength;

		tDirRecordDateTime m_RecDateTime;

		ckcore::tstring m_FileName;

		Iso9660TreeNode(Iso9660TreeNode *pParent,const ckcore::tchar *szFileName,
			unsigned long ulExtentLocation,unsigned long ulExtentLength,
			unsigned short usVolSeqNumber,unsigned char ucFileFlags,
			unsigned char ucFileUnitSize,unsigned char ucInterleaveGapSize,
			tDirRecordDateTime &RecDateTime) : m_pParent(pParent)
		{
			m_ucFileFlags = ucFileFlags;
			m_ucFileUnitSize = ucFileUnitSize;
			m_ucInterleaveGapSize = ucInterleaveGapSize;
			m_usVolSeqNumber = usVolSeqNumber;
			m_ulExtentLocation = ulExtentLocation;
			m_ulExtentLength = ulExtentLength;

			memcpy(&m_RecDateTime,&RecDateTime,sizeof(tDirRecordDateTime));

			if (szFileName != NULL)
				m_FileName = szFileName;
		}

		~Iso9660TreeNode()
		{
			// Free the children.
			std::vector<Iso9660TreeNode *>::iterator itNode;
			for (itNode = m_Children.begin(); itNode != m_Children.end(); itNode++)
				delete *itNode;

			m_Children.clear();
		}

		Iso9660TreeNode *GetParent()
		{
			return m_pParent;
		}
	};

	class Iso9660Reader
	{
	private:
		ckcore::Log &log_;

		Iso9660TreeNode *m_pRootNode;

		bool ReadDirEntry(ckcore::InStream &InStream,
			std::vector<Iso9660TreeNode *> &DirEntries,
			Iso9660TreeNode *pParentNode,bool bJoliet);

	public:
		Iso9660Reader(ckcore::Log &log);
		~Iso9660Reader();

		bool Read(ckcore::InStream &InStream,unsigned long ulStartSector);

		Iso9660TreeNode *GetRoot()
		{
			return m_pRootNode;
		}

	#ifdef _DEBUG
		void PrintLocalTree(std::vector<std::pair<Iso9660TreeNode *,int> > &DirNodeStack,
			Iso9660TreeNode *pLocalNode,int iIndent);
		void PrintTree();
	#endif
	};
};
