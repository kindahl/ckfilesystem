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
#include <ckcore/types.hh>
#include <ckcore/stream.hh>
#include "ckfilesystem/iso9660.hh"

#define JOLIET_MAX_NAMELEN_NORMAL			 64	// According to Joliet specification.
#define JOLIET_MAX_NAMELEN_RELAXED			101	// 207 bytes = 101 wide characters + 4 wide characters for file version.

namespace ckfilesystem
{
	class Joliet
	{
	private:
		bool m_bIncFileVerInfo;
		int m_iMaxNameLen;

		tVolDescSuppl m_VolDescSuppl;

		wchar_t MakeChar(wchar_t c);
        int LastDelimiterW(const wchar_t *szString,wchar_t cDelimiter);
		void MemStrCopy(unsigned char *szTarget,const wchar_t *szSource,size_t iLen);
		void EmptyStrBuffer(unsigned char *szBuffer,size_t iBufferLen);

		void InitVolDesc();

	public:
		Joliet();
		~Joliet();

		// Change of internal state functions.
		void SetVolumeLabel(const ckcore::tchar *szLabel);
		void SetTextFields(const ckcore::tchar *szSystem,const ckcore::tchar *szVolSetIdent,
			const ckcore::tchar *szPublIdent,const ckcore::tchar *szPrepIdent);
		void SetFileFields(const ckcore::tchar *ucCopyFileIdent,const ckcore::tchar *ucAbstFileIdent,
			const ckcore::tchar *ucBiblIdent);
		void SetIncludeFileVerInfo(bool bIncludeInfo);
		void SetRelaxMaxNameLen(bool bRelaxRestriction);

		// Write functions.
		bool WriteVolDesc(ckcore::OutStream &out_stream,struct tm &ImageCreate,
			unsigned long ulVolSpaceSize,unsigned long ulPathTableSize,
			unsigned long ulPosPathTableL,unsigned long ulPosPathTableM,
			unsigned long ulRootExtentLoc,unsigned long ulDataLen);

		// Helper functions.
		unsigned char WriteFileName(unsigned char *pOutBuffer,const ckcore::tchar *szFileName,bool bIsDir);
		unsigned char CalcFileNameLen(const ckcore::tchar *szFileName,bool bIsDir);
		bool IncludesFileVerInfo();
	};
};
