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
#ifdef _WINDOWS
#include <windows.h>
#endif
#include "ckfilesystem/iso9660.hh"

namespace ckfilesystem
{
	/*
		Identifiers.
	*/
	const char *g_IdentCD = "CD001";
	const char *g_IdentElTorito = "EL TORITO SPECIFICATION";

	/*
		Helper Functions.
	*/
	void Write721(unsigned char *pOut,unsigned short usValue)		// Least significant byte first.
	{
		pOut[0] = usValue & 0xFF;
		pOut[1] = (usValue >> 8) & 0xFF;
	}

	void Write722(unsigned char *pOut,unsigned short usValue)		// Most significant byte first.
	{
		pOut[0] = (usValue >> 8) & 0xFF;
		pOut[1] = usValue & 0xFF;
	}

	void Write723(unsigned char *pOut,unsigned short usValue)		// Both-byte orders.
	{
		pOut[3] = pOut[0] = usValue & 0xFF;
		pOut[2] = pOut[1] = (usValue >> 8) & 0xFF;
	}

	void Write731(unsigned char *pOut,unsigned long ulValue)		// Least significant byte first.
	{
		pOut[0] = (unsigned char)(ulValue & 0xFF);
		pOut[1] = (unsigned char)((ulValue >> 8) & 0xFF);
		pOut[2] = (unsigned char)((ulValue >> 16) & 0xFF);
		pOut[3] = (unsigned char)((ulValue >> 24) & 0xFF);
	}

	void Write732(unsigned char *pOut,unsigned long ulValue)		// Most significant byte first.
	{
		pOut[0] = (unsigned char)((ulValue >> 24) & 0xFF);
		pOut[1] = (unsigned char)((ulValue >> 16) & 0xFF);
		pOut[2] = (unsigned char)((ulValue >> 8) & 0xFF);
		pOut[3] = (unsigned char)(ulValue & 0xFF);
	}

	void Write733(unsigned char *pOut,unsigned long ulValue)		// Both-byte orders.
	{
		pOut[7] = pOut[0] = (unsigned char)(ulValue & 0xFF);
		pOut[6] = pOut[1] = (unsigned char)((ulValue >> 8) & 0xFF);
		pOut[5] = pOut[2] = (unsigned char)((ulValue >> 16) & 0xFF);
		pOut[4] = pOut[3] = (unsigned char)((ulValue >> 24) & 0xFF);
	}

	void Write72(unsigned char *pOut,unsigned short usValue,bool bMSBF)
	{
		if (bMSBF)
			Write722(pOut,usValue);
		else
			Write721(pOut,usValue);
	}

	void Write73(unsigned char *pOut,unsigned long ulValue,bool bMSBF)
	{
		if (bMSBF)
			Write732(pOut,ulValue);
		else
			Write731(pOut,ulValue);
	}

	unsigned short Read721(unsigned char *pOut)		// Least significant byte first.
	{
		return ((unsigned short)pOut[1] << 8) | pOut[0];
	}

	unsigned short Read722(unsigned char *pOut)		// Most significant byte first.
	{
		return ((unsigned short)pOut[0] << 8) | pOut[1];
	}

	unsigned short Read723(unsigned char *pOut)		// Both-byte orders.
	{
		return Read721(pOut);
	}

	unsigned long Read731(unsigned char *pOut)			// Least significant byte first.
	{
		return ((unsigned long)pOut[3] << 24) | ((unsigned long)pOut[2] << 16) |
			((unsigned long)pOut[1] << 8) | pOut[0];
	}

	unsigned long Read732(unsigned char *pOut)			// Most significant byte first.
	{
		return ((unsigned long)pOut[0] << 24) | ((unsigned long)pOut[1] << 16) |
			((unsigned long)pOut[2] << 8) | pOut[3];
	}

	unsigned long Read733(unsigned char *pOut)			// Both-byte orders.
	{
		return Read731(pOut);
	}

	unsigned long BytesToSector(unsigned long ulBytes)
	{
		if (ulBytes == 0)
			return 0;

		unsigned long ulSectors = 1;

		while (ulBytes > ISO9660_SECTOR_SIZE)
		{
			ulBytes -= ISO9660_SECTOR_SIZE;
			ulSectors++;
		}

		return ulSectors;
	}

	unsigned long BytesToSector(ckcore::tuint64 uiBytes)
	{
		if (uiBytes == 0)
			return 0;

		unsigned long ulSectors = 1;

		while (uiBytes > ISO9660_SECTOR_SIZE)
		{
			uiBytes -= ISO9660_SECTOR_SIZE;
			ulSectors++;
		}

		return ulSectors;
	}

	ckcore::tuint64 BytesToSector64(ckcore::tuint64 uiBytes)
	{
		if (uiBytes == 0)
			return 0;

		ckcore::tuint64 uiSectors = 1;

		while (uiBytes > ISO9660_SECTOR_SIZE)
		{
			uiBytes -= ISO9660_SECTOR_SIZE;
			uiSectors++;
		}

		return uiSectors;
	}

	void MakeDateTime(struct tm &Time,tVolDescDateTime &DateTime)
	{
		char szBuffer[5];
		sprintf(szBuffer,"%.4u",Time.tm_year + 1900);
		memcpy(&DateTime.uiYear,szBuffer,4);

		sprintf(szBuffer,"%.2u",Time.tm_mon + 1);
		memcpy(&DateTime.usMonth,szBuffer,2);

		sprintf(szBuffer,"%.2u",Time.tm_mday);
		memcpy(&DateTime.usDay,szBuffer,2);

		sprintf(szBuffer,"%.2u",Time.tm_hour);
		memcpy(&DateTime.usHour,szBuffer,2);

		sprintf(szBuffer,"%.2u",Time.tm_min);
		memcpy(&DateTime.usMinute,szBuffer,2);

        if (Time.tm_sec > 59)
            sprintf(szBuffer,"%.2u",59);
        else
            sprintf(szBuffer,"%.2u",Time.tm_sec);
		memcpy(&DateTime.usSecond,szBuffer,2);

		sprintf(szBuffer,"%.2u",0);
		memcpy(&DateTime.usHundreds,szBuffer,2);

#ifdef _WINDOWS
		TIME_ZONE_INFORMATION tzi;
		GetTimeZoneInformation(&tzi);
		DateTime.ucZone = -(unsigned char)(tzi.Bias/15);
#else
        // FIXME: Add support for Unix time zone information.
        DateTime.ucZone = 0;
#endif
	}

	void MakeDateTime(struct tm &Time,tDirRecordDateTime &DateTime)
	{
		DateTime.ucYear = (unsigned char)Time.tm_year;
		DateTime.ucMonth = (unsigned char)Time.tm_mon + 1;
		DateTime.ucDay = (unsigned char)Time.tm_mday;
		DateTime.ucHour = (unsigned char)Time.tm_hour;
		DateTime.ucMinute = (unsigned char)Time.tm_min;
        DateTime.ucSecond = Time.tm_sec > 59 ? 59 : (unsigned char)Time.tm_sec;

#ifdef _WINDOWS
		TIME_ZONE_INFORMATION tzi;
		GetTimeZoneInformation(&tzi);
		DateTime.ucZone = -(unsigned char)(tzi.Bias/15);
#else
        // FIXME: Add support for Unix time zone information.
        DateTime.ucZone = 0;
#endif
	}

	void MakeDateTime(unsigned short usDate,unsigned short usTime,tDirRecordDateTime &DateTime)
	{
		DateTime.ucYear = ((usDate >> 9) & 0x7F) + 80;
		DateTime.ucMonth = (usDate >> 5) & 0x0F;
		DateTime.ucDay = usDate & 0x1F;
		DateTime.ucHour = (usTime >> 11) & 0x1F;
		DateTime.ucMinute = (usTime >> 5) & 0x3F;
		DateTime.ucSecond = (usTime & 0x1F) << 1;

#ifdef _WINDOWS
		TIME_ZONE_INFORMATION tzi;
		GetTimeZoneInformation(&tzi);
		DateTime.ucZone = -(unsigned char)(tzi.Bias/15);
#else
        // FIXME: Add support for Unix time zone information.
        DateTime.ucZone = 0;
#endif
	}

	void MakeDosDateTime(tDirRecordDateTime &DateTime,unsigned short &usDate,unsigned short &usTime)
	{
		usDate = ((DateTime.ucYear - 80) & 0x7F) << 9;
		usDate |= (DateTime.ucMonth & 0x7F) << 5;
		usDate |= (DateTime.ucDay & 0x1F);

		usTime = (DateTime.ucHour & 0x1F) << 11;
		usTime |= (DateTime.ucMinute & 0x3F) << 5;
		usTime |= (DateTime.ucSecond & 0x1F) >> 1;
	}

	Iso9660::Iso9660()
	{
		m_InterLevel = LEVEL_1;
		m_bRelaxMaxDirLevel = false;
		m_bIncFileVerInfo = true;	// Include ";1" file version information.

		InitVolDescPrimary();
		InitVolDescSetTerm();
	}

	Iso9660::~Iso9660()
	{
	}

	/*
		Convert the specified character to an a-character (appendix A).
	*/
	char Iso9660::MakeCharA(char c)
	{
		char cResult = toupper(c);

		// Make sure that it's a valid character, otherwise return '_'.
		if ((cResult >= 0x20 && cResult <= 0x22) ||
			(cResult >= 0x25 && cResult <= 0x39) ||
			(cResult >= 0x41 && cResult <= 0x5A) || cResult == 0x5F)
			return cResult;

		return '_';
	}

	/*
		Convert the specified character to a d-character (appendix A).
	*/
	char Iso9660::MakeCharD(char c)
	{
		char cResult = toupper(c);

		// Make sure that it's a valid character, otherwise return '_'.
		if ((cResult >= 0x30 && cResult <= 0x39) ||
			(cResult >= 0x41 && cResult <= 0x5A) || cResult == 0x5F)
			return cResult;

		return '_';
	}

    /*
     * Fins the last specified delimiter in the specified string.
     */
    int Iso9660::LastDelimiterA(const char *szString,char cDelimiter)
    {    
        int iLength = (int)strlen(szString);

        for (int i = iLength - 1; i >= 0; i--)
        {
            if (szString[i] == cDelimiter)
                return i;
        }

        return -1;
    }

	/*
		Performs a memory copy from szSource to szTarget, all characters
		in szTarget will be A-characters.
	*/
	void Iso9660::MemStrCopyA(unsigned char *szTarget,const char *szSource,size_t iLength)
	{
		for (size_t i = 0; i < iLength; i++)
			szTarget[i] = MakeCharA(szSource[i]);
	}

	/*
		Performs a memory copy from szSource to szTarget, all characters
		in szTarget will be D-characters.
	*/
	void Iso9660::MemStrCopyD(unsigned char *szTarget,const char *szSource,size_t iLength)
	{
		for (size_t i = 0; i < iLength; i++)
			szTarget[i] = MakeCharD(szSource[i]);
	}

	/*
		Converts the input file name to a valid ISO level 1 file name. This means:
		 - A maximum of 12 characters.
		 - A file extension of at most 3 characters.
		 - A file name of at most 8 characters.
	*/
	unsigned char Iso9660::WriteFileNameL1(unsigned char *pOutBuffer,const ckcore::tchar *szFileName)
	{
		int iFileNameLen = (int)ckcore::string::astrlen(szFileName);
		unsigned char ucLength = 0;

		char *szMultiFileName;
	#ifdef _UNICODE
		szMultiFileName = new char [iFileNameLen + 1];
		ckcore::string::utf16_to_ansi(szFileName,szMultiFileName,iFileNameLen + 1);
	#else
		szMultiFileName = (char *)szFileName;
	#endif

		int iExtDelimiter = LastDelimiterA(szMultiFileName,'.');
		if (iExtDelimiter == -1)
		{
			size_t iMax = iFileNameLen < 8 ? iFileNameLen : 8;
			for (size_t i = 0; i < iMax; i++)
				pOutBuffer[i] = MakeCharD(szMultiFileName[i]);
			
			ucLength = (unsigned char)iMax;
			pOutBuffer[iMax] = '\0';
		}
		else
		{
			int iExtLen = (int)iFileNameLen - iExtDelimiter - 1;
			if (iExtLen > 3)	// Level one support a maxmimum file extension of length 3.
				iExtLen = 3;

			size_t iMax = iExtDelimiter < 8 ? iExtDelimiter : 8;
			for (size_t i = 0; i < iMax; i++)
				pOutBuffer[i] = MakeCharD(szMultiFileName[i]);

			pOutBuffer[iMax] = '.';

			// Copy the extension.
			for (size_t i = iMax + 1; i < iMax + iExtLen + 1; i++)
				pOutBuffer[i] = MakeCharD(szMultiFileName[++iExtDelimiter]);

			ucLength = (unsigned char)iMax + (unsigned char)iExtLen + 1;
			pOutBuffer[ucLength] = '\0';
		}

	#ifdef _UNICODE
		delete [] szMultiFileName;
	#endif

		return ucLength;
	}

	unsigned char Iso9660::WriteFileNameGeneric(unsigned char *pOutBuffer,const ckcore::tchar *szFileName,
												 int iMaxLen)
	{
		int iFileNameLen = (int)ckcore::string::astrlen(szFileName);
		unsigned char ucLength = 0;

	#ifdef _UNICODE
		char *szMultiFileName = new char [iFileNameLen + 1];
		ckcore::string::utf16_to_ansi(szFileName,szMultiFileName,iFileNameLen + 1);
	#else
		char *szMultiFileName = (char *)szFileName;
	#endif

		int iExtDelimiter = LastDelimiterA(szMultiFileName,'.');
		if (iExtDelimiter == -1)
		{
			size_t iMax = iFileNameLen < iMaxLen ? iFileNameLen : iMaxLen;
			for (size_t i = 0; i < iMax; i++)
				pOutBuffer[i] = MakeCharD(szMultiFileName[i]);
			
			ucLength = (unsigned char)iMax;
			pOutBuffer[iMax] = '\0';
		}
		else
		{
			int iExtLen = (int)iFileNameLen - iExtDelimiter - 1;
			if (iExtLen > iMaxLen - 1)	// The file can at most contain an extension of length iMaxLen - 1 characters.
				iExtLen = iMaxLen - 1;

			size_t iMax = iExtDelimiter < (iMaxLen - iExtLen) ? iExtDelimiter : (iMaxLen - 1 - iExtLen);
			for (size_t i = 0; i < iMax; i++)
				pOutBuffer[i] = MakeCharD(szMultiFileName[i]);

			pOutBuffer[iMax] = '.';

			// Copy the extension.
			for (size_t i = iMax + 1; i < iMax + iExtLen + 1; i++)
				pOutBuffer[i] = MakeCharD(szMultiFileName[++iExtDelimiter]);

			ucLength = (unsigned char)iMax + (unsigned char)iExtLen + 1;
			pOutBuffer[ucLength] = '\0';
		}

	#ifdef _UNICODE
		delete [] szMultiFileName;
	#endif

		return ucLength;
	}

	/*
		Converts the input file name to a valid ISO level 2 and above file name. This means:
		 - A maximum of 31 characters.
	*/
	unsigned char Iso9660::WriteFileNameL2(unsigned char *pOutBuffer,const ckcore::tchar *szFileName)
	{
		return WriteFileNameGeneric(pOutBuffer,szFileName,31);
	}

	unsigned char Iso9660::WriteFileName1999(unsigned char *pOutBuffer,const ckcore::tchar *szFileName)
	{
		return WriteFileNameGeneric(pOutBuffer,szFileName,ISO9660_MAX_NAMELEN_1999);
	}

	unsigned char Iso9660::WriteDirNameL1(unsigned char *pOutBuffer,const ckcore::tchar *szDirName)
	{
		int iDirNameLen = (int)ckcore::string::astrlen(szDirName);
		int iMax = iDirNameLen < 8 ? iDirNameLen : 8;

	#ifdef _UNICODE
		char *szMultiDirName = new char [iDirNameLen + 1];
		ckcore::string::utf16_to_ansi(szDirName,szMultiDirName,iDirNameLen + 1);
	#else
		char *szMultiDirName = (char *)szDirName;
	#endif

		for (size_t i = 0; i < (size_t)iMax; i++)
			pOutBuffer[i] = MakeCharD(szMultiDirName[i]);
			
		pOutBuffer[iMax] = '\0';

	#ifdef _UNICODE
		delete [] szMultiDirName;
	#endif

		return iMax;
	}

	unsigned char Iso9660::WriteDirNameGeneric(unsigned char *pOutBuffer,const ckcore::tchar *szDirName,
												int iMaxLen)
	{
		int iDirNameLen = (int)ckcore::string::astrlen(szDirName);
		int iMax = iDirNameLen < iMaxLen ? iDirNameLen : iMaxLen;

	#ifdef _UNICODE
		char *szMultiDirName = new char [iDirNameLen + 1];
		ckcore::string::utf16_to_ansi(szDirName,szMultiDirName,iDirNameLen + 1);
	#else
		char *szMultiDirName = (char *)szDirName;
	#endif

		for (size_t i = 0; i < (size_t)iMax; i++)
			pOutBuffer[i] = MakeCharD(szMultiDirName[i]);
			
		pOutBuffer[iMax] = '\0';

	#ifdef _UNICODE
		delete [] szMultiDirName;
	#endif

		return iMax;
	}

	unsigned char Iso9660::WriteDirNameL2(unsigned char *pOutBuffer,const ckcore::tchar *szDirName)
	{
		return WriteDirNameGeneric(pOutBuffer,szDirName,31);
	}

	unsigned char Iso9660::WriteDirName1999(unsigned char *pOutBuffer,const ckcore::tchar *szDirName)
	{
		return WriteDirNameGeneric(pOutBuffer,szDirName,ISO9660_MAX_NAMELEN_1999);
	}

	unsigned char Iso9660::CalcFileNameLenL1(const ckcore::tchar *szFileName)
	{
		unsigned char szTempBuffer[13];
		return WriteFileNameL1(szTempBuffer,szFileName);
	}

	unsigned char Iso9660::CalcFileNameLenL2(const ckcore::tchar *szFileName)
	{
		size_t iFileNameLen = ckcore::string::astrlen(szFileName);
		if (iFileNameLen < 31)
			return (unsigned char)iFileNameLen;

		return 31;
	}

	unsigned char Iso9660::CalcFileNameLen1999(const ckcore::tchar *szFileName)
	{
		size_t iFileNameLen = ckcore::string::astrlen(szFileName);
		if (iFileNameLen < ISO9660_MAX_NAMELEN_1999)
			return (unsigned char)iFileNameLen;

		return ISO9660_MAX_NAMELEN_1999;
	}

	unsigned char Iso9660::CalcDirNameLenL1(const ckcore::tchar *szDirName)
	{
		size_t iDirNameLen = ckcore::string::astrlen(szDirName);
		if (iDirNameLen < 8)
			return (unsigned char)iDirNameLen;

		return 8;
	}

	unsigned char Iso9660::CalcDirNameLenL2(const ckcore::tchar *szDirName)
	{
		size_t iDirNameLen = ckcore::string::astrlen(szDirName);
		if (iDirNameLen < 31)
			return (unsigned char)iDirNameLen;

		return 31;
	}

	unsigned char Iso9660::CalcDirNameLen1999(const ckcore::tchar *szDirName)
	{
		size_t iDirNameLen = ckcore::string::astrlen(szDirName);
		if (iDirNameLen < ISO9660_MAX_NAMELEN_1999)
			return (unsigned char)iDirNameLen;

		return ISO9660_MAX_NAMELEN_1999;
	}

	void Iso9660::InitVolDescPrimary()
	{
		// Clear memory.
		memset(&m_VolDescPrimary,0,sizeof(m_VolDescPrimary));
		memset(m_VolDescPrimary.ucSysIdentifier,0x20,sizeof(m_VolDescPrimary.ucSysIdentifier));
		memset(m_VolDescPrimary.ucVolIdentifier,0x20,sizeof(m_VolDescPrimary.ucVolIdentifier));
		memset(m_VolDescPrimary.ucVolSetIdentifier,0x20,sizeof(m_VolDescPrimary.ucVolSetIdentifier));
		memset(m_VolDescPrimary.ucPublIdentifier,0x20,sizeof(m_VolDescPrimary.ucPublIdentifier));
		memset(m_VolDescPrimary.ucPrepIdentifier,0x20,sizeof(m_VolDescPrimary.ucPrepIdentifier));
		memset(m_VolDescPrimary.ucAppIdentifier,0x20,sizeof(m_VolDescPrimary.ucAppIdentifier));
		memset(m_VolDescPrimary.ucCopyFileIdentifier,0x20,sizeof(m_VolDescPrimary.ucCopyFileIdentifier));
		memset(m_VolDescPrimary.ucAbstFileIdentifier,0x20,sizeof(m_VolDescPrimary.ucAbstFileIdentifier));
		memset(m_VolDescPrimary.ucBiblFileIdentifier,0x20,sizeof(m_VolDescPrimary.ucBiblFileIdentifier));

		// Set primary volume descriptor header.
		m_VolDescPrimary.ucType = VOLDESCTYPE_PRIM_VOL_DESC;
		m_VolDescPrimary.ucVersion = 1;
		m_VolDescPrimary.ucFileStructVer = 1;
		memcpy(m_VolDescPrimary.ucIdentifier,g_IdentCD,sizeof(m_VolDescPrimary.ucIdentifier));	

		// Set the root directory record.
		m_VolDescPrimary.RootDirRecord.ucDirRecordLen = 34;
		m_VolDescPrimary.RootDirRecord.ucFileFlags = DIRRECORD_FILEFLAG_DIRECTORY;
		m_VolDescPrimary.RootDirRecord.ucFileIdentifierLen = 1;	// One byte is always allocated in the tDirRecord structure.

		// Set application identifier.
		memset(m_VolDescPrimary.ucAppData,0x20,sizeof(m_VolDescPrimary.ucAppData));
		char szAppIdentifier[] = { 0x49,0x4E,0x46,0x52,0x41,0x52,0x45,0x43,0x4F,
			0x52,0x44,0x45,0x52,0x20,0x28,0x43,0x29,0x20,0x32,0x30,0x30,0x36,0x2D,
			0x32,0x30,0x30,0x38,0x20,0x43,0x48,0x52,0x49,0x53,0x54,0x49,0x41,0x4E,
			0x20,0x4B,0x49,0x4E,0x44,0x41,0x48,0x4C };
		memcpy(m_VolDescPrimary.ucAppIdentifier,szAppIdentifier,45);
	}

	void Iso9660::InitVolDescSetTerm()
	{
		// Clear memory.
		memset(&m_VolDescSetTerm,0,sizeof(m_VolDescSetTerm));

		// Set volume descriptor set terminator header.
		m_VolDescSetTerm.ucType = VOLDESCTYPE_VOL_DESC_SET_TERM;
		m_VolDescSetTerm.ucVersion = 1;
		memcpy(m_VolDescSetTerm.ucIdentifier,g_IdentCD,sizeof(m_VolDescSetTerm.ucIdentifier));
	}

	void Iso9660::SetVolumeLabel(const ckcore::tchar *szLabel)
	{
		size_t iLabelLen = ckcore::string::astrlen(szLabel);
		size_t iLabelCopyLen = iLabelLen < 32 ? iLabelLen : 32;

		memset(m_VolDescPrimary.ucVolIdentifier,0x20,sizeof(m_VolDescPrimary.ucVolIdentifier));

	#ifdef _UNICODE
		char szMultiLabel[33];
		ckcore::string::utf16_to_ansi(szLabel,szMultiLabel,sizeof(szMultiLabel));
		MemStrCopyD(m_VolDescPrimary.ucVolIdentifier,szMultiLabel,iLabelCopyLen);
	#else
		MemStrCopyD(m_VolDescPrimary.ucVolIdentifier,szLabel,iLabelCopyLen);
	#endif
	}

	void Iso9660::SetTextFields(const ckcore::tchar *szSystem,const ckcore::tchar *szVolSetIdent,
								 const ckcore::tchar *szPublIdent,const ckcore::tchar *szPrepIdent)
	{
		size_t iSystemLen = ckcore::string::astrlen(szSystem);
		size_t iVolSetIdentLen = ckcore::string::astrlen(szVolSetIdent);
		size_t iPublIdentLen = ckcore::string::astrlen(szPublIdent);
		size_t iPrepIdentLen = ckcore::string::astrlen(szPrepIdent);

		size_t iSystemCopyLen = iSystemLen < 32 ? iSystemLen : 32;
		size_t iVolSetIdentCopyLen = iVolSetIdentLen < 128 ? iVolSetIdentLen : 128;
		size_t iPublIdentCopyLen = iPublIdentLen < 128 ? iPublIdentLen : 128;
		size_t iPrepIdentCopyLen = iPrepIdentLen < 128 ? iPrepIdentLen : 128;

		memset(m_VolDescPrimary.ucSysIdentifier,0x20,sizeof(m_VolDescPrimary.ucSysIdentifier));
		memset(m_VolDescPrimary.ucVolSetIdentifier,0x20,sizeof(m_VolDescPrimary.ucVolSetIdentifier));
		memset(m_VolDescPrimary.ucPublIdentifier,0x20,sizeof(m_VolDescPrimary.ucPublIdentifier));
		memset(m_VolDescPrimary.ucPrepIdentifier,0x20,sizeof(m_VolDescPrimary.ucPrepIdentifier));

	#ifdef _UNICODE
		char szMultiSystem[33];
		char szMultiVolSetIdent[129];
		char szMultiPublIdent[129];
		char szMultiPrepIdent[129];

		ckcore::string::utf16_to_ansi(szSystem,szMultiSystem,sizeof(szMultiSystem));
		ckcore::string::utf16_to_ansi(szVolSetIdent,szMultiVolSetIdent,sizeof(szMultiVolSetIdent));
		ckcore::string::utf16_to_ansi(szPublIdent,szMultiPublIdent,sizeof(szMultiPublIdent));
		ckcore::string::utf16_to_ansi(szPrepIdent,szMultiPrepIdent,sizeof(szMultiPrepIdent));

		MemStrCopyA(m_VolDescPrimary.ucSysIdentifier,szMultiSystem,iSystemCopyLen);
		MemStrCopyD(m_VolDescPrimary.ucVolSetIdentifier,szMultiVolSetIdent,iVolSetIdentCopyLen);
		MemStrCopyA(m_VolDescPrimary.ucPublIdentifier,szMultiPublIdent,iPublIdentCopyLen);
		MemStrCopyA(m_VolDescPrimary.ucPrepIdentifier,szMultiPrepIdent,iPrepIdentCopyLen);
	#else
		MemStrCopyA(m_VolDescPrimary.ucSysIdentifier,szSystem,iSystemCopyLen);
		MemStrCopyD(m_VolDescPrimary.ucVolSetIdentifier,szVolSetIdent,iVolSetIdentCopyLen);
		MemStrCopyA(m_VolDescPrimary.ucPublIdentifier,szPublIdent,iPublIdentCopyLen);
		MemStrCopyA(m_VolDescPrimary.ucPrepIdentifier,szPrepIdent,iPrepIdentCopyLen);
	#endif
	}

	void Iso9660::SetFileFields(const ckcore::tchar *szCopyFileIdent,
								 const ckcore::tchar *szAbstFileIdent,
								 const ckcore::tchar *szBiblFileIdent)
	{
		size_t iCopyFileIdentLen = ckcore::string::astrlen(szCopyFileIdent);
		size_t iAbstFileIdentLen = ckcore::string::astrlen(szAbstFileIdent);
		size_t iBiblFileIdentLen = ckcore::string::astrlen(szBiblFileIdent);

		size_t iCopyFileIdentCopyLen = iCopyFileIdentLen < 37 ? iCopyFileIdentLen : 37;
		size_t iAbstFileIdentCopyLen = iAbstFileIdentLen < 37 ? iAbstFileIdentLen : 37;
		size_t iBiblFileIdentCopyLen = iBiblFileIdentLen < 37 ? iBiblFileIdentLen : 37;

		memset(m_VolDescPrimary.ucCopyFileIdentifier,0x20,sizeof(m_VolDescPrimary.ucCopyFileIdentifier));
		memset(m_VolDescPrimary.ucAbstFileIdentifier,0x20,sizeof(m_VolDescPrimary.ucAbstFileIdentifier));
		memset(m_VolDescPrimary.ucBiblFileIdentifier,0x20,sizeof(m_VolDescPrimary.ucBiblFileIdentifier));

	#ifdef _UNICODE
		char szMultiCopyFileIdent[38];
		char szMultiAbstFileIdent[38];
		char szMultiBiblFileIdent[38];

		ckcore::string::utf16_to_ansi(szCopyFileIdent,szMultiCopyFileIdent,sizeof(szMultiCopyFileIdent));
		ckcore::string::utf16_to_ansi(szAbstFileIdent,szMultiAbstFileIdent,sizeof(szMultiAbstFileIdent));
		ckcore::string::utf16_to_ansi(szBiblFileIdent,szMultiBiblFileIdent,sizeof(szMultiBiblFileIdent));

		MemStrCopyD(m_VolDescPrimary.ucCopyFileIdentifier,szMultiCopyFileIdent,iCopyFileIdentCopyLen);
		MemStrCopyD(m_VolDescPrimary.ucAbstFileIdentifier,szMultiAbstFileIdent,iAbstFileIdentCopyLen);
		MemStrCopyD(m_VolDescPrimary.ucBiblFileIdentifier,szMultiBiblFileIdent,iBiblFileIdentCopyLen);
	#else
		MemStrCopyD(m_VolDescPrimary.ucCopyFileIdentifier,szCopyFileIdent,iCopyFileIdentCopyLen);
		MemStrCopyD(m_VolDescPrimary.ucAbstFileIdentifier,szAbstFileIdent,iAbstFileIdentCopyLen);
		MemStrCopyD(m_VolDescPrimary.ucBiblFileIdentifier,szBiblFileIdent,iBiblFileIdentCopyLen);
	#endif
	}

	void Iso9660::SetInterchangeLevel(InterLevel inter_level)
	{
		m_InterLevel = inter_level;
	}

	void Iso9660::SetRelaxMaxDirLevel(bool bRelaxRestriction)
	{
		m_bRelaxMaxDirLevel = bRelaxRestriction;
	}

	void Iso9660::SetIncludeFileVerInfo(bool bIncludeInfo)
	{
		m_bIncFileVerInfo = bIncludeInfo;
	}

	bool Iso9660::WriteVolDescPrimary(ckcore::OutStream &out_stream,struct tm &ImageCreate,
		unsigned long ulVolSpaceSize,unsigned long ulPathTableSize,unsigned long ulPosPathTableL,
		unsigned long ulPosPathTableM,unsigned long ulRootExtentLoc,unsigned long ulDataLen)
	{
		// Initialize the primary volume descriptor.
		Write733(m_VolDescPrimary.ucVolSpaceSize,ulVolSpaceSize);		// Volume size in sectors.
		Write723(m_VolDescPrimary.ucVolSetSize,1);		// Only one disc in the volume set.
		Write723(m_VolDescPrimary.ucVolSeqNumber,1);	// This is the first disc in the volume set.
		Write723(m_VolDescPrimary.ucLogicalBlockSize,ISO9660_SECTOR_SIZE);
		Write733(m_VolDescPrimary.ucPathTableSize,ulPathTableSize);	// Path table size in bytes.
		Write731(m_VolDescPrimary.ucPathTableTypeL,ulPosPathTableL);	// Start sector of LSBF path table.
		Write732(m_VolDescPrimary.ucPathTableTypeM,ulPosPathTableM);	// Start sector of MSBF path table.

		Write733(m_VolDescPrimary.RootDirRecord.ucExtentLocation,ulRootExtentLoc);
		Write733(m_VolDescPrimary.RootDirRecord.ucDataLen,ulDataLen);
		Write723(m_VolDescPrimary.RootDirRecord.ucVolSeqNumber,1);	// The file extent is on the first volume set.

		// Time information.
		MakeDateTime(ImageCreate,m_VolDescPrimary.RootDirRecord.RecDateTime);

		MakeDateTime(ImageCreate,m_VolDescPrimary.CreateDateTime);
		memcpy(&m_VolDescPrimary.ModDateTime,&m_VolDescPrimary.CreateDateTime,sizeof(tVolDescDateTime));

		memset(&m_VolDescPrimary.ExpDateTime,'0',sizeof(tVolDescDateTime));
		m_VolDescPrimary.ExpDateTime.ucZone = 0x00;
		memset(&m_VolDescPrimary.EffectiveDateTime,'0',sizeof(tVolDescDateTime));
		m_VolDescPrimary.EffectiveDateTime.ucZone = 0x00;

		// Write the primary volume descriptor.
		ckcore::tint64 iProcessed = out_stream.Write(&m_VolDescPrimary,sizeof(m_VolDescPrimary));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(m_VolDescPrimary))
			return false;

		return true;
	}

	bool Iso9660::WriteVolDescSuppl(ckcore::OutStream &out_stream,struct tm &ImageCreate,
		unsigned long ulVolSpaceSize,unsigned long ulPathTableSize,unsigned long ulPosPathTableL,
		unsigned long ulPosPathTableM,unsigned long ulRootExtentLoc,unsigned long ulDataLen)
	{
		if (m_InterLevel == ISO9660_1999)
		{
			tVolDescSuppl SupplDesc;
			memcpy(&SupplDesc,&m_VolDescPrimary,sizeof(tVolDescSuppl));

			// Update the version information.
			m_VolDescPrimary.ucType = VOLDESCTYPE_SUPPL_VOL_DESC;
			m_VolDescPrimary.ucVersion = 2;			// ISO9660:1999
			m_VolDescPrimary.ucFileStructVer = 2;	// ISO9660:1999

			// Rewrite the values from the primary volume descriptor. We can't guarantee that
			// WriteVolDescPrimary has been called before this function call, even though it
			// should have.
			Write733(SupplDesc.ucVolSpaceSize,ulVolSpaceSize);		// Volume size in sectors.
			Write723(SupplDesc.ucVolSetSize,1);		// Only one disc in the volume set.
			Write723(SupplDesc.ucVolSeqNumber,1);	// This is the first disc in the volume set.
			Write723(SupplDesc.ucLogicalBlockSize,ISO9660_SECTOR_SIZE);
			Write733(SupplDesc.ucPathTableSize,ulPathTableSize);	// Path table size in bytes.
			Write731(SupplDesc.ucPathTableTypeL,ulPosPathTableL);	// Start sector of LSBF path table.
			Write732(SupplDesc.ucPathTableTypeM,ulPosPathTableM);	// Start sector of MSBF path table.

			Write733(SupplDesc.RootDirRecord.ucExtentLocation,ulRootExtentLoc);
			Write733(SupplDesc.RootDirRecord.ucDataLen,ulDataLen);
			Write723(SupplDesc.RootDirRecord.ucVolSeqNumber,1);	// The file extent is on the first volume set.

			// Time information.
			MakeDateTime(ImageCreate,SupplDesc.RootDirRecord.RecDateTime);

			MakeDateTime(ImageCreate,SupplDesc.CreateDateTime);
			memcpy(&SupplDesc.ModDateTime,&SupplDesc.CreateDateTime,sizeof(tVolDescDateTime));

			memset(&SupplDesc.ExpDateTime,'0',sizeof(tVolDescDateTime));
			SupplDesc.ExpDateTime.ucZone = 0x00;
			memset(&SupplDesc.EffectiveDateTime,'0',sizeof(tVolDescDateTime));
			SupplDesc.EffectiveDateTime.ucZone = 0x00;

			// Write the primary volume descriptor.
			ckcore::tint64 iProcessed = out_stream.Write(&SupplDesc,sizeof(SupplDesc));
			if (iProcessed == -1)
				return false;
			if (iProcessed != sizeof(SupplDesc))
				return false;

			return true;
		}

		return false;
	}

	bool Iso9660::WriteVolDescSetTerm(ckcore::OutStream &out_stream)
	{
		// Write volume descriptor set terminator.
		ckcore::tint64 iProcessed = out_stream.Write(&m_VolDescSetTerm,sizeof(m_VolDescSetTerm));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(m_VolDescSetTerm))
			return false;

		return true;
	}

	unsigned char Iso9660::WriteFileName(unsigned char *pOutBuffer,const ckcore::tchar *szFileName,bool bIsDir)
	{
		switch (m_InterLevel)
		{
			case LEVEL_1:
			default:
				if (bIsDir)
				{
					return WriteDirNameL1(pOutBuffer,szFileName);
				}
				else
				{
					unsigned char ucFileNameLen = WriteFileNameL1(pOutBuffer,szFileName);

					if (m_bIncFileVerInfo)
					{
						pOutBuffer[ucFileNameLen++] = ';';
						pOutBuffer[ucFileNameLen++] = '1';
					}

					return ucFileNameLen;
				}
				break;

			case LEVEL_2:
				if (bIsDir)
				{
					return WriteDirNameL2(pOutBuffer,szFileName);
				}
				else
				{
					unsigned char ucFileNameLen = WriteFileNameL2(pOutBuffer,szFileName);

					if (m_bIncFileVerInfo)
					{
						pOutBuffer[ucFileNameLen++] = ';';
						pOutBuffer[ucFileNameLen++] = '1';
					}

					return ucFileNameLen;
				}
				break;

			case ISO9660_1999:
				if (bIsDir)
					return WriteDirName1999(pOutBuffer,szFileName);
				else
					return WriteFileName1999(pOutBuffer,szFileName);
		}
	}

	/**
		Determines the length of the compatible file name generated from the
		specified file name. A compatible file name is a file name that is
		generated from the specified file name that is supported by the current
		file system configuration.
		@param szFileName the origial file name.
		@param bIsDir if true, szFileName is assumed to be a directory name.
		@return the length of the compatible file name.
	*/
	unsigned char Iso9660::CalcFileNameLen(const ckcore::tchar *szFileName,bool bIsDir)
	{
		switch (m_InterLevel)
		{
			case LEVEL_1:
			default:
				if (bIsDir)
				{
					return CalcDirNameLenL1(szFileName);
				}
				else
				{
					unsigned char ucFileNameLen = CalcFileNameLenL1(szFileName);

					if (m_bIncFileVerInfo)
						ucFileNameLen += 2;

					return ucFileNameLen;
				}
				break;

			case LEVEL_2:
				if (bIsDir)
				{
					return CalcDirNameLenL2(szFileName);
				}
				else
				{
					unsigned char ucFileNameLen = CalcFileNameLenL2(szFileName);

					if (m_bIncFileVerInfo)
						ucFileNameLen += 2;

					return ucFileNameLen;
				}
				break;

			case ISO9660_1999:
				if (bIsDir)
					return CalcDirNameLen1999(szFileName);
				else
					return CalcFileNameLen1999(szFileName);
		}
	}

	/**
		Obtains the maximum numbe of directory levels supported by the current
		file system configuration.
		@return maximum number of allowed directory levels.
	*/
	unsigned char Iso9660::GetMaxDirLevel()
	{
		if (m_bRelaxMaxDirLevel)
		{
			return ISO9660_MAX_DIRLEVEL_1999;
		}
		else
		{
			switch (m_InterLevel)
			{
				case LEVEL_1:
				case LEVEL_2:
				default:
					return ISO9660_MAX_DIRLEVEL_NORMAL;

				case ISO9660_1999:
					return ISO9660_MAX_DIRLEVEL_1999;
			}
		}
	}

	/**
		Checks whether the file system has a supplementary volume descriptor or not.
		@return true if the file system has a supplementary volume descriptor.
	*/
	bool Iso9660::HasVolDescSuppl()
	{
		return m_InterLevel == ISO9660_1999;
	}

	/**
		Checks whether the file system allows files to be fragmented (multiple
		extents) or not.
		@return true if the file system allows fragmentation.
	*/
	bool Iso9660::AllowsFragmentation()
	{
		return m_InterLevel == LEVEL_3;
	}

	/**
		Returns true if the file names includes the two character file version
		information (;1).
	*/
	bool Iso9660::IncludesFileVerInfo()
	{
		return m_bIncFileVerInfo;
	}
};
