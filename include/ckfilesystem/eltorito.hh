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
#include <string>
#include <vector>
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include "ckfilesystem/sectorstream.hh"

// Maximum values of unsigned short + 1 + the default boot image.
#define ELTORITO_MAX_BOOTIMAGE_COUNT			0xFFFF + 2

#define ELTORITO_IO_BUFFER_SIZE					0x10000

#pragma pack(1)	// Force byte alignment.

/*
	Boot Catalog Entries.
*/
#define ELTORITO_PLATFORM_80X86					0
#define ELTORITO_PLATFORM_POWERPC				1
#define ELTORITO_PLATFORM_MAC					2

typedef struct
{
	unsigned char ucHeader;
	unsigned char ucPlatform;
	unsigned short usReserved1;
	unsigned char ucManufacturer[24];
	unsigned short usCheckSum;
	unsigned char ucKeyByte1;		// Must be 0x55.
	unsigned char ucKeyByte2;		// Must be 0xAA.
} tElToritoValiEntry;

#define ELTORITO_BOOTINDICATOR_BOOTABLE			0x88
#define ELTORITO_BOOTINDICATOR_NONBOOTABLE		0x00

#define ELTORITO_EMULATION_NONE					0
#define ELTORITO_EMULATION_DISKETTE12			1
#define ELTORITO_EMULATION_DISKETTE144			2
#define ELTORITO_EMULATION_DISKETTE288			3
#define ELTORITO_EMULATION_HARDDISK				4

typedef struct
{
	unsigned char ucBootIndicator;
	unsigned char ucEmulation;
	unsigned short usLoadSegment;
	unsigned char ucSysType;		// Must be a copy of byte 5 (System Type) from boot image partition table.
	unsigned char ucUnused1;
	unsigned short usSectorCount;
	unsigned long ulLoadSecAddr;
	unsigned char ucUnused2[20];
} tElToritoDefEntry;

#define ELTORITO_HEADER_NORMAL					0x90
#define ELTORITO_HEADER_FINAL					0x91

typedef struct
{
	unsigned char ucHeader;
	unsigned char ucPlatform;
	unsigned short usNumSecEntries;
	unsigned char ucIndentifier[28];
} tElToritoSecHeader;

typedef struct
{
	unsigned char ucBootIndicator;
	unsigned char ucEmulation;
	unsigned short usLoadSegment;
	unsigned char ucSysType;		// Must be a copy of byte 5 (System Type) from boot image partition table.
	unsigned char ucUnused1;
	unsigned short usSectorCount;
	unsigned long ulLoadSecAddr;
	unsigned char ucSelCriteria;
	unsigned char ucUnused2[19];
} tElToritoSecEntry;

/*
	Structures for reading the master boot record of a boot image.
*/
#define MBR_PARTITION_COUNT						4

typedef struct
{
	unsigned char ucBootIndicator;
	unsigned char ucPartStartCHS[3];
	unsigned char ucPartType;
	unsigned char ucPartEndCHS[3];
	unsigned long ulStartLBA;
	unsigned long ulSecCount;
} tMasterBootRecPart;

typedef struct
{
	unsigned char ucCodeArea[440];
	unsigned long ulOptDiscSig;
	unsigned short usPad;
	tMasterBootRecPart Partitions[MBR_PARTITION_COUNT];
	unsigned char ucSignature1;
	unsigned char ucSignature2;
} tMasterBootRec;

#pragma pack()	// Switch back to normal alignment.

namespace ckFileSystem
{
	class ElToritoImage
	{
	public:
		enum eEmulation
		{
			EMULATION_NONE,
			EMULATION_FLOPPY,
			EMULATION_HARDDISK
		};

		ckcore::tstring m_FullPath;
		bool m_bBootable;
		eEmulation m_Emulation;
		unsigned short m_usLoadSegment;
		unsigned short m_usSectorCount;

		// Needs to be calculated in a separate pass.
		unsigned long m_ulDataSecPos;	// Sector number of first sector containing data.

		ElToritoImage(const ckcore::tchar *szFullPath,bool bBootable,eEmulation Emulation,
			unsigned short usLoadSegment,unsigned short usSectorCount)
		{
			m_FullPath = szFullPath;
			m_bBootable = bBootable;
			m_Emulation = Emulation;
			m_usLoadSegment = usLoadSegment;
			m_usSectorCount = usSectorCount;

			m_ulDataSecPos = 0;
		}
	};

	class ElTorito
	{
	private:
		ckcore::Log *m_pLog;

		std::vector<ElToritoImage *> m_BootImages;

		bool ReadSysTypeMBR(const ckcore::tchar *szFullPath,unsigned char &ucSysType);

		bool WriteBootImage(SectorOutStream *pOutStream,const ckcore::tchar *szFileName);

	public:
		ElTorito(ckcore::Log *pLog);
		~ElTorito();

		bool WriteBootRecord(SectorOutStream *pOutStream,unsigned long ulBootCatSecPos);
		bool WriteBootCatalog(SectorOutStream *pOutStream);
		bool WriteBootImages(SectorOutStream *pOutStream);

		bool AddBootImageNoEmu(const ckcore::tchar *szFullPath,bool bBootable,
			unsigned short usLoadSegment,unsigned short usSectorCount);
		bool AddBootImageFloppy(const ckcore::tchar *szFullPath,bool bBootable);
		bool AddBootImageHardDisk(const ckcore::tchar *szFullPath,bool bBootable);

		bool CalculateFileSysData(ckcore::tuint64 uiStartSec,
			ckcore::tuint64 &uiLastSec);

		ckcore::tuint64 GetBootCatSize();
		ckcore::tuint64 GetBootDataSize();
		ckcore::tuint64 GetBootImageCount();
	};
};
