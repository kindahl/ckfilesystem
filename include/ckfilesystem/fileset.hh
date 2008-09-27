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
#include <ckcore/types.hh>
#include <ckcore/string.hh>

namespace ckfilesystem
{
	/*
		Describes a file that should be included in the disc image.
	*/
	class FileDescriptor
	{
	public:
		enum
		{
			FLAG_DIRECTORY = 0x01,
			FLAG_IMPORTED = 0x02
		};

		FileDescriptor(const ckcore::tchar *szInternalPath,const ckcore::tchar *szExternalPath,
			ckcore::tuint64 uiFileSize,unsigned char ucFlags = 0,void *pData = NULL)
		{
			m_ucFlags = ucFlags;
			m_uiFileSize = uiFileSize;
			m_InternalPath = szInternalPath;
			m_ExternalPath = szExternalPath;
			m_pData = pData;
		}

		unsigned char m_ucFlags;
		ckcore::tuint64 m_uiFileSize;
		ckcore::tstring m_InternalPath;		// Path in disc image.
		ckcore::tstring m_ExternalPath;		// Path on hard drive.

		void *m_pData;					// Pointer to a user-defined structure, designed for CIso9660TreeNode
	};

	/*
		Sorts the set of files according to the ECMA-119 standard.
	*/
	class FileComparator
	{
	private:
		bool m_bDvdVideo;

		/*
			Returns a weight of the specified file name, a ligher file should
			be placed heigher in the directory hierarchy.
		*/
		unsigned long GetFileWeight(const ckcore::tchar *szFullPath) const
		{
			unsigned long ulWeight = 0xFFFFFFFF;

			// Quick test for optimization.
			if (szFullPath[1] == 'V')
			{
				if (!ckcore::string::astrcmp(szFullPath,ckT("/VIDEOckTS")))	// The VIDEO_TS folder should be first.
					ulWeight = 0;
				else if (!ckcore::string::astrncmp(szFullPath,ckT("/VIDEOckTS/"),10))
				{
					const ckcore::tchar *szFileName = szFullPath + 10;

					if (!ckcore::string::astrncmp(szFileName,ckT("VIDEOckTS"),8))
					{
						ulWeight -= 0x80000000;

						const ckcore::tchar *szFileExt = szFileName + 9;
						if (!ckcore::string::astrcmp(szFileExt,ckT("IFO")))
							ulWeight -= 3;
						else if (!ckcore::string::astrcmp(szFileExt,ckT("VOB")))
							ulWeight -= 2;
						else if (!ckcore::string::astrcmp(szFileExt,ckT("BUP")))
							ulWeight -= 1;
					}
					else if (!ckcore::string::astrncmp(szFileName,ckT("VTS_"),4))
					{
						ulWeight -= 0x40000000;

						// Just a safety measure.
						if (ckcore::string::astrlen(szFileName) < 64)
						{
							ckcore::tchar szFileExt[64];
							unsigned long ulNum = 0,ulSubNum = 0;

							if (asscanf(szFileName,ckT("VTS_%u_%u.%[^\0]"),&ulNum,&ulSubNum,szFileExt) == 3)
							{
								// The first number is worth the most, the lower the lighter.
								ulWeight -= 0xFFFFFF - (ulNum << 8);

								if (!ckcore::string::astrcmp(szFileExt,ckT("IFO")))
								{
									ulWeight -= 0xFF;
								}
								else if (!ckcore::string::astrcmp(szFileExt,ckT("VOB")))
								{
									ulWeight -= 0x0F - ulSubNum;
								}
								else if (!ckcore::string::astrcmp(szFileExt,ckT("BUP")))
								{
									ulWeight -= 1;
								}
							}
						}
					}
				}
			}

			return ulWeight;
		}

	public:
		/*
			@param bDvdVideo set to true to use DVD-Video compatible sorting.
		*/
		FileComparator(bool bDvdVideo) : m_bDvdVideo(bDvdVideo)
		{
		}

		static int Level(const FileDescriptor &Item)
		{
			const ckcore::tchar *szFullPath = Item.m_InternalPath.c_str();

			int iLevel = 0;
			for (size_t i = 0; i < (size_t)ckcore::string::astrlen(szFullPath); i++)
			{
				if (szFullPath[i] == '/' || szFullPath[i] == '\\')
					iLevel++;
			}

			if (Item.m_ucFlags & FileDescriptor::FLAG_DIRECTORY)
				iLevel++;

			return iLevel;
		}

		bool operator() (const FileDescriptor &Item1,const FileDescriptor &Item2) const
		{
			if (m_bDvdVideo)
			{
				unsigned long ulWeight1 = GetFileWeight(Item1.m_InternalPath.c_str());
				unsigned long ulWeight2 = GetFileWeight(Item2.m_InternalPath.c_str());

				if (ulWeight1 != ulWeight2)
				{
					if (ulWeight1 < ulWeight2)
						return true;
					else
						return false;
				}
			}

			int iLevelItem1 = Level(Item1);
			int iLevelItem2 = Level(Item2);

			if (iLevelItem1 < iLevelItem2)
				return true;
			else if (iLevelItem1 == iLevelItem2)
				return ckcore::string::astrcmp(Item1.m_InternalPath.c_str(),Item2.m_InternalPath.c_str()) < 0;
			else
				return false;
		}
	};

	typedef std::set<ckfilesystem::FileDescriptor,ckfilesystem::FileComparator> FileSet;
};
