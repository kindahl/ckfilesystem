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
	unsigned char header;
	unsigned char platform;
	unsigned short res1;
	unsigned char manufacturer[24];
	unsigned short checksum;
	unsigned char key_byte1;		// Must be 0x55.
	unsigned char key_byte2;		// Must be 0xAA.
} teltorito_valientry;

#define ELTORITO_BOOTINDICATOR_BOOTABLE			0x88
#define ELTORITO_BOOTINDICATOR_NONBOOTABLE		0x00

#define ELTORITO_EMULATION_NONE					0
#define ELTORITO_EMULATION_DISKETTE12			1
#define ELTORITO_EMULATION_DISKETTE144			2
#define ELTORITO_EMULATION_DISKETTE288			3
#define ELTORITO_EMULATION_HARDDISK				4

typedef struct
{
	unsigned char boot_indicator;
	unsigned char emulation;
	unsigned short load_segment;
	unsigned char sys_type;		// Must be a copy of byte 5 (System Type) from boot image partition table.
	unsigned char unused1;
	unsigned short sec_count;
	unsigned long load_sec_addr;
	unsigned char unused2[20];
} teltorito_defentry;

#define ELTORITO_HEADER_NORMAL					0x90
#define ELTORITO_HEADER_FINAL					0x91

typedef struct
{
	unsigned char header;
	unsigned char platform;
	unsigned short num_sec_entries;
	unsigned char ident[28];
} teltorito_sec_header;

typedef struct
{
	unsigned char boot_indicator;
	unsigned char emulation;
	unsigned short load_segment;
	unsigned char sys_type;		// Must be a copy of byte 5 (System Type) from boot image partition table.
	unsigned char unused1;
	unsigned short sec_count;
	unsigned long load_sec_addr;
	unsigned char sel_criteria;
	unsigned char unused2[19];
} teltorito_sec_entry;

/*
	Structures for reading the master boot record of a boot image.
*/
#define MBR_PARTITION_COUNT						4

typedef struct
{
	unsigned char boot_indicator;
	unsigned char part_start_chs[3];
	unsigned char part_type;
	unsigned char part_end_chs[3];
	unsigned long start_lba;
	unsigned long sec_count;
} teltorito_mbr_part;

typedef struct
{
	unsigned char code_area[440];
	unsigned long opt_disc_sig;
	unsigned short pad;
	teltorito_mbr_part partitions[MBR_PARTITION_COUNT];
	unsigned char signature1;
	unsigned char signature2;
} teltorito_mbr;

#pragma pack()	// Switch back to normal alignment.

namespace ckfilesystem
{
	class ElToritoImage
	{
	public:
		enum Emulation
		{
			EMULATION_NONE,
			EMULATION_FLOPPY,
			EMULATION_HARDDISK
		};

		ckcore::tstring m_FullPath;
		bool m_bBootable;
		Emulation m_Emulation;
		unsigned short m_load_segment;
		unsigned short m_sec_count;

		// Needs to be calculated in a separate pass.
		unsigned long m_ulDataSecPos;	// Sector number of first sector containing data.

		ElToritoImage(const ckcore::tchar *szFullPath,bool bBootable,Emulation emulation,
			unsigned short load_segment,unsigned short sec_count)
		{
			m_FullPath = szFullPath;
			m_bBootable = bBootable;
			m_Emulation = emulation;
			m_load_segment = load_segment;
			m_sec_count = sec_count;

			m_ulDataSecPos = 0;
		}
	};

	class ElTorito
	{
	private:
		ckcore::Log &log_;

		std::vector<ElToritoImage *> m_BootImages;

		bool ReadSysTypeMBR(const ckcore::tchar *szFullPath,unsigned char &sys_type);

		bool WriteBootImage(SectorOutStream &out_stream,const ckcore::tchar *szFileName);

	public:
		ElTorito(ckcore::Log &log);
		~ElTorito();

		bool WriteBootRecord(SectorOutStream &out_stream,unsigned long ulBootCatSecPos);
		bool WriteBootCatalog(SectorOutStream &out_stream);
		bool WriteBootImages(SectorOutStream &out_stream);

		bool AddBootImageNoEmu(const ckcore::tchar *szFullPath,bool bBootable,
			unsigned short load_segment,unsigned short sec_count);
		bool AddBootImageFloppy(const ckcore::tchar *szFullPath,bool bBootable);
		bool AddBootImageHardDisk(const ckcore::tchar *szFullPath,bool bBootable);

		bool CalculateFileSysData(ckcore::tuint64 uiStartSec,
			ckcore::tuint64 &uiLastSec);

		ckcore::tuint64 GetBootCatSize();
		ckcore::tuint64 GetBootDataSize();
		ckcore::tuint64 GetBootImageCount();
	};
};
