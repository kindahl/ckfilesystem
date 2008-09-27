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

#include <iostream>
#include <ckcore/string.hh>
#include "ckfilesystem/joliet.hh"

namespace ckfilesystem
{
	Joliet::Joliet()
	{
		m_bIncFileVerInfo = true;	// Include ";1" file version information.
		m_iMaxNameLen = 64;			// According to Joliet specification.

		InitVolDesc();
	}

	Joliet::~Joliet()
	{
	}

	/*
		Guaraties that the returned character is allowed by the Joliet file system.
	*/
	wchar_t Joliet::MakeChar(wchar_t c)
	{
		if (c == '*' || c == '/' || c == ':' || c == ';' || c == '?' || c == '\\')
			return '_';

		return c;
	}

    /*
     * Find the last delimiter of the specified kind in the specified string.
     */
    int Joliet::LastDelimiterW(const wchar_t *szString,wchar_t cDelimiter)
    {    
        int iLength = (int)wcslen(szString);

        for (int i = iLength - 1; i >= 0; i--)
        {
            if (szString[i] == cDelimiter)
                return i;
        }

        return -1;
    }

	/*
		Copies the source string to the target buffer assuring that all characters
		in the source string are allowed by the Joliet file system. iLen should
		be the length of the source string in wchar_t characters.
	*/
	void Joliet::MemStrCopy(unsigned char *szTarget,const wchar_t *szSource,size_t iLen)
	{
		for (size_t i = 0,j = 0; j < iLen; j++)
		{
			wchar_t cSafe = MakeChar(szSource[j]);

			szTarget[i++] = cSafe >> 8;
			szTarget[i++] = cSafe & 0xFF;
		}
	}

	void Joliet::EmptyStrBuffer(unsigned char *szBuffer,size_t iBufferLen)
	{
		for (size_t i = 0; i < iBufferLen; i += 2)
		{
			szBuffer[0] = 0x00;
			szBuffer[1] = 0x20;
		}
	}

	void Joliet::InitVolDesc()
	{
		// Clear memory.
		memset(&m_VolDescSuppl,0,sizeof(m_VolDescSuppl));
		EmptyStrBuffer(m_VolDescSuppl.ucSysIdentifier,sizeof(m_VolDescSuppl.ucSysIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucVolIdentifier,sizeof(m_VolDescSuppl.ucVolIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucVolSetIdentifier,sizeof(m_VolDescSuppl.ucVolSetIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucPublIdentifier,sizeof(m_VolDescSuppl.ucPublIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucPrepIdentifier,sizeof(m_VolDescSuppl.ucPrepIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucAppIdentifier,sizeof(m_VolDescSuppl.ucAppIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucCopyFileIdentifier,sizeof(m_VolDescSuppl.ucCopyFileIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucAbstFileIdentifier,sizeof(m_VolDescSuppl.ucAbstFileIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucBiblFileIdentifier,sizeof(m_VolDescSuppl.ucBiblFileIdentifier));

		// Set primary volume descriptor header.
		m_VolDescSuppl.ucType = VOLDESCTYPE_SUPPL_VOL_DESC;
		m_VolDescSuppl.ucVersion = 1;
		m_VolDescSuppl.ucFileStructVer = 1;
		memcpy(m_VolDescSuppl.ucIdentifier,g_IdentCD,sizeof(m_VolDescSuppl.ucIdentifier));	

		// Always use Joliet level 3.
		m_VolDescSuppl.ucEscapeSeq[0] = 0x25;
		m_VolDescSuppl.ucEscapeSeq[1] = 0x2F;
		m_VolDescSuppl.ucEscapeSeq[2] = 0x45;

		// Set the root directory record.
		m_VolDescSuppl.RootDirRecord.ucDirRecordLen = 34;
		m_VolDescSuppl.RootDirRecord.ucFileFlags = DIRRECORD_FILEFLAG_DIRECTORY;
		m_VolDescSuppl.RootDirRecord.ucFileIdentifierLen = 1;	// One byte is always allocated in the tDirRecord structure.

		// Set application identifier.
		memset(m_VolDescSuppl.ucAppData,0x20,sizeof(m_VolDescSuppl.ucAppData));
		char szAppIdentifier[] = { 0x00,0x49,0x00,0x6E,0x00,0x66,0x00,0x72,0x00,0x61,
			0x00,0x52,0x00,0x65,0x00,0x63,0x00,0x6F,0x00,0x72,0x00,0x64,0x00,0x65,0x00,0x72,
			0x00,0x20,0x00,0x28,0x00,0x43,0x00,0x29,0x00,0x20,0x00,0x32,0x00,0x30,0x00,0x30,
			0x00,0x36,0x00,0x2D,0x00,0x32,0x00,0x30,0x00,0x30,0x00,0x38,0x00,0x20,0x00,0x43,
			0x00,0x68,0x00,0x72,0x00,0x69,0x00,0x73,0x00,0x74,0x00,0x69,0x00,0x61,0x00,0x6E,
			0x00,0x20,0x00,0x4B,0x00,0x69,0x00,0x6E,0x00,0x64,0x00,0x61,0x00,0x68,0x00,0x6C };
		memcpy(m_VolDescSuppl.ucAppIdentifier,szAppIdentifier,90);
	}

	bool Joliet::WriteVolDesc(ckcore::OutStream *pOutStream,struct tm &ImageCreate,
		unsigned long ulVolSpaceSize,unsigned long ulPathTableSize,unsigned long ulPosPathTableL,
		unsigned long ulPosPathTableM,unsigned long ulRootExtentLoc,unsigned long ulDataLen)
	{
		// Initialize the supplementary volume descriptor.
		Write733(m_VolDescSuppl.ucVolSpaceSize,ulVolSpaceSize);		// Volume size in sectors.
		Write723(m_VolDescSuppl.ucVolSetSize,1);		// Only one disc in the volume set.
		Write723(m_VolDescSuppl.ucVolSeqNumber,1);		// This is the first disc in the volume set.
		Write723(m_VolDescSuppl.ucLogicalBlockSize,ISO9660_SECTOR_SIZE);
		Write733(m_VolDescSuppl.ucPathTableSize,ulPathTableSize);	// Path table size in bytes.
		Write731(m_VolDescSuppl.ucPathTableTypeL,ulPosPathTableL);	// Start sector of LSBF path table.
		Write732(m_VolDescSuppl.ucPathTableTypeM,ulPosPathTableM);	// Start sector of MSBF path table.

		Write733(m_VolDescSuppl.RootDirRecord.ucExtentLocation,ulRootExtentLoc);
		Write733(m_VolDescSuppl.RootDirRecord.ucDataLen,ulDataLen);
		Write723(m_VolDescSuppl.RootDirRecord.ucVolSeqNumber,1);	// The file extent is on the first volume set.

		// Time information.
		MakeDateTime(ImageCreate,m_VolDescSuppl.RootDirRecord.RecDateTime);

		MakeDateTime(ImageCreate,m_VolDescSuppl.CreateDateTime);
		memcpy(&m_VolDescSuppl.ModDateTime,&m_VolDescSuppl.CreateDateTime,sizeof(tVolDescDateTime));

		memset(&m_VolDescSuppl.ExpDateTime,'0',sizeof(tVolDescDateTime));
		m_VolDescSuppl.ExpDateTime.ucZone = 0x00;
		memset(&m_VolDescSuppl.EffectiveDateTime,'0',sizeof(tVolDescDateTime));
		m_VolDescSuppl.EffectiveDateTime.ucZone = 0x00;

		// Write the supplementary volume descriptor.
		ckcore::tint64 iProcessed = pOutStream->Write(&m_VolDescSuppl,sizeof(m_VolDescSuppl));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(m_VolDescSuppl))
			return false;

		return true;
	}

	void Joliet::SetVolumeLabel(const ckcore::tchar *szLabel)
	{
		size_t iLabelLen = ckcore::string::astrlen(szLabel);
		size_t iLabelCopyLen = iLabelLen < 16 ? iLabelLen : 16;

		EmptyStrBuffer(m_VolDescSuppl.ucVolIdentifier,sizeof(m_VolDescSuppl.ucVolIdentifier));

	#ifdef _UNICODE
		MemStrCopy(m_VolDescSuppl.ucVolIdentifier,szLabel,iLabelCopyLen);
	#else
		wchar_t szWideLabel[17];
		ckcore::string::ansi_to_utf16(szLabel,szWideLabel,sizeof(szWideLabel) / sizeof(wchar_t));
		MemStrCopy(m_VolDescSuppl.ucVolIdentifier,szWideLabel,iLabelCopyLen);
	#endif
	}

	void Joliet::SetTextFields(const ckcore::tchar *szSystem,const ckcore::tchar *szVolSetIdent,
										 const ckcore::tchar *szPublIdent,const ckcore::tchar *szPrepIdent)
	{
		size_t iSystemLen = ckcore::string::astrlen(szSystem);
		size_t iVolSetIdentLen = ckcore::string::astrlen(szVolSetIdent);
		size_t iPublIdentLen = ckcore::string::astrlen(szPublIdent);
		size_t iPrepIdentLen = ckcore::string::astrlen(szPrepIdent);

		size_t iSystemCopyLen = iSystemLen < 16 ? iSystemLen : 16;
		size_t iVolSetIdentCopyLen = iVolSetIdentLen < 64 ? iVolSetIdentLen : 64;
		size_t iPublIdentCopyLen = iPublIdentLen < 64 ? iPublIdentLen : 64;
		size_t iPrepIdentCopyLen = iPrepIdentLen < 64 ? iPrepIdentLen : 64;

		EmptyStrBuffer(m_VolDescSuppl.ucSysIdentifier,sizeof(m_VolDescSuppl.ucSysIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucVolSetIdentifier,sizeof(m_VolDescSuppl.ucVolSetIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucPublIdentifier,sizeof(m_VolDescSuppl.ucPublIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucPrepIdentifier,sizeof(m_VolDescSuppl.ucPrepIdentifier));

	#ifdef _UNICODE
		MemStrCopy(m_VolDescSuppl.ucSysIdentifier,szSystem,iSystemCopyLen);
		MemStrCopy(m_VolDescSuppl.ucVolSetIdentifier,szVolSetIdent,iVolSetIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucPublIdentifier,szPublIdent,iPublIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucPrepIdentifier,szPrepIdent,iPrepIdentCopyLen);
	#else
		wchar_t szWideSystem[17];
		wchar_t szWideVolSetIdent[65];
		wchar_t szWidePublIdent[65];
		wchar_t szWidePrepIdent[65];

		ckcore::string::ansi_to_utf16(szSystem,szWideSystem,sizeof(szWideSystem) / sizeof(wchar_t));
		ckcore::string::ansi_to_utf16(szVolSetIdent,szWideVolSetIdent,sizeof(szWideVolSetIdent) / sizeof(wchar_t));
		ckcore::string::ansi_to_utf16(szPublIdent,szWidePublIdent,sizeof(szWidePublIdent) / sizeof(wchar_t));
		ckcore::string::ansi_to_utf16(szPrepIdent,szWidePrepIdent,sizeof(szWidePrepIdent) / sizeof(wchar_t));

		MemStrCopy(m_VolDescSuppl.ucSysIdentifier,szWideSystem,iSystemCopyLen);
		MemStrCopy(m_VolDescSuppl.ucVolSetIdentifier,szWideVolSetIdent,iVolSetIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucPublIdentifier,szWidePublIdent,iPublIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucPrepIdentifier,szWidePrepIdent,iPrepIdentCopyLen);
	#endif
	}

	void Joliet::SetFileFields(const ckcore::tchar *szCopyFileIdent,
										 const ckcore::tchar *szAbstFileIdent,
										 const ckcore::tchar *szBiblFileIdent)
	{
		size_t iCopyFileIdentLen = ckcore::string::astrlen(szCopyFileIdent);
		size_t iAbstFileIdentLen = ckcore::string::astrlen(szAbstFileIdent);
		size_t iBiblFileIdentLen = ckcore::string::astrlen(szBiblFileIdent);

		size_t iCopyFileIdentCopyLen = iCopyFileIdentLen < 18 ? iCopyFileIdentLen : 18;
		size_t iAbstFileIdentCopyLen = iAbstFileIdentLen < 18 ? iAbstFileIdentLen : 18;
		size_t iBiblFileIdentCopyLen = iBiblFileIdentLen < 18 ? iBiblFileIdentLen : 18;

		EmptyStrBuffer(m_VolDescSuppl.ucCopyFileIdentifier,sizeof(m_VolDescSuppl.ucCopyFileIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucAbstFileIdentifier,sizeof(m_VolDescSuppl.ucAbstFileIdentifier));
		EmptyStrBuffer(m_VolDescSuppl.ucBiblFileIdentifier,sizeof(m_VolDescSuppl.ucBiblFileIdentifier));

	#ifdef _UNICODE
		MemStrCopy(m_VolDescSuppl.ucCopyFileIdentifier,szCopyFileIdent,iCopyFileIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucAbstFileIdentifier,szAbstFileIdent,iAbstFileIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucBiblFileIdentifier,szBiblFileIdent,iBiblFileIdentCopyLen);
	#else
		wchar_t szWideCopyFileIdent[19];
		wchar_t szWideAbstFileIdent[19];
		wchar_t szWideBiblFileIdent[19];

		ckcore::string::ansi_to_utf16(szCopyFileIdent,szWideCopyFileIdent,sizeof(szWideCopyFileIdent) / sizeof(wchar_t));
		ckcore::string::ansi_to_utf16(szAbstFileIdent,szWideAbstFileIdent,sizeof(szWideAbstFileIdent) / sizeof(wchar_t));
		ckcore::string::ansi_to_utf16(szBiblFileIdent,szWideBiblFileIdent,sizeof(szWideBiblFileIdent) / sizeof(wchar_t));

		MemStrCopy(m_VolDescSuppl.ucCopyFileIdentifier,szWideCopyFileIdent,iCopyFileIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucAbstFileIdentifier,szWideAbstFileIdent,iAbstFileIdentCopyLen);
		MemStrCopy(m_VolDescSuppl.ucBiblFileIdentifier,szWideBiblFileIdent,iBiblFileIdentCopyLen);
	#endif
	}

	void Joliet::SetIncludeFileVerInfo(bool bIncludeInfo)
	{
		m_bIncFileVerInfo = bIncludeInfo;
	}

	void Joliet::SetRelaxMaxNameLen(bool bRelaxRestriction)
	{
		if (bRelaxRestriction)
			m_iMaxNameLen = JOLIET_MAX_NAMELEN_RELAXED;
		else
			m_iMaxNameLen = JOLIET_MAX_NAMELEN_NORMAL;
	}

	unsigned char Joliet::WriteFileName(unsigned char *pOutBuffer,const ckcore::tchar *szFileName,bool bIsDir)
	{
#ifndef _UNICODE
		wchar_t szWideFileName[JOLIET_MAX_NAMELEN_RELAXED + 1];
		ckcore::string::ansi_to_utf16(szFileName,szWideFileName,sizeof(szWideFileName) / sizeof(wchar_t));
#else
		const wchar_t *szWideFileName = szFileName;
#endif

		int iFileNameLen = (int)wcslen(szWideFileName),iMax = 0;

		if (iFileNameLen > m_iMaxNameLen)
		{
			int iExtDelimiter = LastDelimiterW(szWideFileName,'.');
			if (iExtDelimiter != -1)
			{
				int iExtLen = (int)iFileNameLen - iExtDelimiter - 1;
				if (iExtLen > m_iMaxNameLen - 1)	// The file can at most contain an extension of length m_iMaxNameLen - 1 characters.
					iExtLen = m_iMaxNameLen - 1;

				// Copy the file name.
				iMax = iExtDelimiter < (m_iMaxNameLen - iExtLen) ? iExtDelimiter : (m_iMaxNameLen - 1 - iExtLen);

				MemStrCopy(pOutBuffer,szWideFileName,iMax);

				int iOutPos = iMax << 1;
				pOutBuffer[iOutPos++] = 0x00;
				pOutBuffer[iOutPos++] = '.';

				// Copy the extension.
				MemStrCopy(pOutBuffer + iOutPos,szWideFileName + iExtDelimiter + 1,iExtLen);

				iMax = m_iMaxNameLen;
			}
			else
			{
				iMax = m_iMaxNameLen;

				MemStrCopy(pOutBuffer,szWideFileName,iMax);
			}
		}
		else
		{
			iMax = iFileNameLen;

			MemStrCopy(pOutBuffer,szWideFileName,iMax);
		}

		if (!bIsDir && m_bIncFileVerInfo)
		{
			int iOutPos = iMax << 1;
			pOutBuffer[iOutPos + 0] = 0x00;
			pOutBuffer[iOutPos + 1] = ';';
			pOutBuffer[iOutPos + 2] = 0x00;
			pOutBuffer[iOutPos + 3] = '1';

			iMax += 2;
		}

		return iMax;
	}

	unsigned char Joliet::CalcFileNameLen(const ckcore::tchar *szFileName,bool bIsDir)
	{
		/*size_t iNameLen = ckcore::string::astrlen(szFileName);
		if (iNameLen < m_iMaxNameLen)
			return (unsigned char)iNameLen;

		return m_iMaxNameLen;*/

		size_t iNameLen = ckcore::string::astrlen(szFileName);
		if (iNameLen >= (size_t)m_iMaxNameLen)
			iNameLen = m_iMaxNameLen;

		if (!bIsDir && m_bIncFileVerInfo)
			iNameLen += 2;

		return (unsigned char)iNameLen;
	}

	/**
		Returns true if the file names includes the two character file version
		information (;1).
	*/
	bool Joliet::IncludesFileVerInfo()
	{
		return m_bIncFileVerInfo;
	}
};
