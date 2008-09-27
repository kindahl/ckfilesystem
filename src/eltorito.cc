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

#include <ckcore/filestream.hh>
#include "ckfilesystem/eltorito.hh"
#include "ckfilesystem/iso9660.hh"

namespace ckfilesystem
{
	ElTorito::ElTorito(ckcore::Log &log) : log_(log)
	{
	}

	ElTorito::~ElTorito()
	{
		// Free the children.
		std::vector<ElToritoImage *>::iterator itImage;
		for (itImage = m_BootImages.begin(); itImage != m_BootImages.end(); itImage++)
			delete *itImage;

		m_BootImages.clear();
	}

	bool ElTorito::ReadSysTypeMBR(const ckcore::tchar *szFullPath,unsigned char &sys_type)
	{
		// Find the system type in the path table located in the MBR.
		ckcore::FileInStream InFile(szFullPath);
		if (!InFile.Open())
		{
			log_.PrintLine(ckT("  Error: Unable to obtain file handle to \"%s\"."),szFullPath);
			return false;
		}

		teltorito_mbr MBR;

		ckcore::tint64 iProcessed = InFile.Read(&MBR,sizeof(teltorito_mbr));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(teltorito_mbr) ||
			(MBR.signature1 != 0x55 || MBR.signature2 != 0xAA))
		{
			log_.PrintLine(ckT("  Error: Unable to locate MBR in default boot image."));
			return false;
		}

		size_t iUsedPartition = -1;
		for (size_t i = 0; i < MBR_PARTITION_COUNT; i++)
		{
			// Look for the first used partition.
			if (MBR.partitions[i].part_type != 0)
			{
				if (iUsedPartition != -1)
				{
					log_.PrintLine(ckT("  Error: Invalid boot image, it contains more than one partition."));
					return false;
				}
				else
				{
					iUsedPartition = i;
				}
			}
		}

		sys_type = MBR.partitions[iUsedPartition].part_type;
		return true;
	}

	bool ElTorito::WriteBootRecord(SectorOutStream &out_stream,unsigned long ulBootCatSecPos)
	{
		tiso_voldesc_eltorito_record BootRecord;
		memset(&BootRecord,0,sizeof(tiso_voldesc_eltorito_record));

		BootRecord.type = 0;
		memcpy(BootRecord.ident,g_IdentCD,sizeof(BootRecord.ident));
		BootRecord.version = 1;
		memcpy(BootRecord.boot_sys_ident,g_IdentElTorito,strlen(g_IdentElTorito));
		BootRecord.boot_cat_ptr = ulBootCatSecPos;

		ckcore::tint64 iProcessed = out_stream.Write(&BootRecord,sizeof(BootRecord));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(BootRecord))
			return false;

		return true;
	}

	bool ElTorito::WriteBootCatalog(SectorOutStream &out_stream)
	{
		char szManufacturer[] = { 0x49,0x4E,0x46,0x52,0x41,0x52,0x45,0x43,0x4F,0x52,0x44,0x45,0x52 };
		teltorito_valientry ValidationEntry;
		memset(&ValidationEntry,0,sizeof(teltorito_valientry));

		ValidationEntry.header = 0x01;
		ValidationEntry.platform = ELTORITO_PLATFORM_80X86;
		memcpy(ValidationEntry.manufacturer,szManufacturer,13);
		ValidationEntry.key_byte1 = 0x55;
		ValidationEntry.key_byte2 = 0xAA;

		// Calculate check sum.
		int iCheckSum = 0;
		unsigned char *pEntryPtr = (unsigned char *)&ValidationEntry;
		for (size_t i = 0; i < sizeof(teltorito_valientry); i += 2) {
			iCheckSum += (unsigned int)pEntryPtr[i];
			iCheckSum += ((unsigned int)pEntryPtr[i + 1]) << 8;
		}

		ValidationEntry.checksum = -iCheckSum;

		ckcore::tint64 iProcessed = out_stream.Write(&ValidationEntry,sizeof(ValidationEntry));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(ValidationEntry))
			return false;

		// Write the default boot entry.
		if (m_BootImages.size() < 1)
			return false;

		ElToritoImage *pDefImage = m_BootImages[0];

		teltorito_defentry DefBootEntry;
		memset(&DefBootEntry,0,sizeof(teltorito_defentry));

		DefBootEntry.boot_indicator = pDefImage->m_bBootable ?
			ELTORITO_BOOTINDICATOR_BOOTABLE : ELTORITO_BOOTINDICATOR_NONBOOTABLE;
		DefBootEntry.load_segment = pDefImage->m_load_segment;
		DefBootEntry.sec_count = pDefImage->m_sec_count;
		DefBootEntry.load_sec_addr = pDefImage->m_ulDataSecPos;

		switch (pDefImage->m_Emulation)
		{
			case ElToritoImage::EMULATION_NONE:
				DefBootEntry.emulation = ELTORITO_EMULATION_NONE;
				DefBootEntry.sys_type = 0;
				break;

			case ElToritoImage::EMULATION_FLOPPY:
				switch (ckcore::File::Size(pDefImage->m_FullPath.c_str()))
				{
					case 1200 * 1024:
						DefBootEntry.emulation = ELTORITO_EMULATION_DISKETTE12;
						break;
					case 1440 * 1024:
						DefBootEntry.emulation = ELTORITO_EMULATION_DISKETTE144;
						break;
					case 2880 * 1024:
						DefBootEntry.emulation = ELTORITO_EMULATION_DISKETTE288;
						break;
					default:
						return false;
				}

				DefBootEntry.sys_type = 0;
				break;

			case ElToritoImage::EMULATION_HARDDISK:
				DefBootEntry.emulation = ELTORITO_EMULATION_HARDDISK;

				if (!ReadSysTypeMBR(pDefImage->m_FullPath.c_str(),DefBootEntry.sys_type))
					return false;
				break;
		}

		iProcessed = out_stream.Write(&DefBootEntry,sizeof(DefBootEntry));
		if (iProcessed == -1)
			return false;
		if (iProcessed != sizeof(DefBootEntry))
			return false;

		// Write the rest of the boot images.
		teltorito_sec_header SecHeader;
		teltorito_sec_entry SecEntry;

		unsigned short usNumImages = (unsigned short)m_BootImages.size();
		for (unsigned short i = 1; i < usNumImages; i++)
		{
			ElToritoImage *pCurImage = m_BootImages[i];

			// Write section header.
			memset(&SecHeader,0,sizeof(teltorito_sec_header));

			SecHeader.header = i == (usNumImages - 1) ?
				ELTORITO_HEADER_FINAL : ELTORITO_HEADER_NORMAL;
			SecHeader.platform = ELTORITO_PLATFORM_80X86;
			SecHeader.num_sec_entries = 1;

			char szIdentifier[16];
			sprintf(szIdentifier,"IMAGE%u",i + 1);
			memcpy(SecHeader.ident,szIdentifier,strlen(szIdentifier));

			iProcessed = out_stream.Write(&SecHeader,sizeof(SecHeader));
			if (iProcessed == -1)
				return false;
			if (iProcessed != sizeof(SecHeader))
				return false;

			// Write the section entry.
			memset(&SecEntry,0,sizeof(teltorito_sec_entry));

			SecEntry.boot_indicator = pCurImage->m_bBootable ?
				ELTORITO_BOOTINDICATOR_BOOTABLE : ELTORITO_BOOTINDICATOR_NONBOOTABLE;
			SecEntry.load_segment = pCurImage->m_load_segment;
			SecEntry.sec_count = pCurImage->m_sec_count;
			SecEntry.load_sec_addr = pCurImage->m_ulDataSecPos;

			switch (pCurImage->m_Emulation)
			{
				case ElToritoImage::EMULATION_NONE:
					SecEntry.emulation = ELTORITO_EMULATION_NONE;
					SecEntry.sys_type = 0;
					break;

				case ElToritoImage::EMULATION_FLOPPY:
					switch (ckcore::File::Size(pCurImage->m_FullPath.c_str()))
					{
						case 1200 * 1024:
							SecEntry.emulation = ELTORITO_EMULATION_DISKETTE12;
							break;
						case 1440 * 1024:
							SecEntry.emulation = ELTORITO_EMULATION_DISKETTE144;
							break;
						case 2880 * 1024:
							SecEntry.emulation = ELTORITO_EMULATION_DISKETTE288;
							break;
						default:
							return false;
					}

					SecEntry.sys_type = 0;
					break;

				case ElToritoImage::EMULATION_HARDDISK:
					SecEntry.emulation = ELTORITO_EMULATION_HARDDISK;

					if (!ReadSysTypeMBR(pCurImage->m_FullPath.c_str(),SecEntry.sys_type))
						return false;
					break;
			}

			iProcessed = out_stream.Write(&SecEntry,sizeof(SecEntry));
			if (iProcessed == -1)
				return false;
			if (iProcessed != sizeof(SecEntry))
				return false;
		}

		if (out_stream.GetAllocated() != 0)
			out_stream.PadSector();

		return true;
	}

	bool ElTorito::WriteBootImage(SectorOutStream &out_stream,const ckcore::tchar *szFileName)
	{
		ckcore::FileInStream FileStream(szFileName);
		if (!FileStream.Open())
		{
			log_.PrintLine(ckT("  Error: Unable to obtain file handle to \"%s\"."),szFileName);
			return false;
		}

		char szBuffer[ELTORITO_IO_BUFFER_SIZE];

		while (!FileStream.End())
		{
			ckcore::tint64 iProcessed = FileStream.Read(szBuffer,ELTORITO_IO_BUFFER_SIZE);
			if (iProcessed == -1)
			{
				log_.PrintLine(ckT("  Error: Unable read file: %s."),szFileName);
				return false;
			}

			if (out_stream.Write(szBuffer,(ckcore::tuint32)iProcessed) == -1)
			{
				log_.PrintLine(ckT("  Error: Unable write to disc image."));
				return false;
			}
		}

		// Pad the sector.
		if (out_stream.GetAllocated() != 0)
			out_stream.PadSector();

		return true;
	}

	bool ElTorito::WriteBootImages(SectorOutStream &out_stream)
	{
		std::vector<ElToritoImage *>::iterator itImage;
		for (itImage = m_BootImages.begin(); itImage != m_BootImages.end(); itImage++)
		{
			if (!WriteBootImage(out_stream,(*itImage)->m_FullPath.c_str()))
				return false;
		}

		return true;
	}

	bool ElTorito::AddBootImageNoEmu(const ckcore::tchar *szFullPath,bool bBootable,
		unsigned short load_segment,unsigned short sec_count)
	{
		if (m_BootImages.size() >= ELTORITO_MAX_BOOTIMAGE_COUNT)
			return false;

		if (!ckcore::File::Exist(szFullPath))
			return false;

		m_BootImages.push_back(new ElToritoImage(
			szFullPath,bBootable,ElToritoImage::EMULATION_NONE,load_segment,sec_count));
		return true;
	}

	bool ElTorito::AddBootImageFloppy(const ckcore::tchar *szFullPath,bool bBootable)
	{
		if (m_BootImages.size() >= ELTORITO_MAX_BOOTIMAGE_COUNT)
			return false;

		if (!ckcore::File::Exist(szFullPath))
			return false;

		ckcore::tuint64 uiFileSize = ckcore::File::Size(szFullPath);
		if (uiFileSize != 1200 * 1024 && uiFileSize != 1440 * 1024 && uiFileSize != 2880 * 1024)
		{
			log_.PrintLine(ckT("  Error: Invalid file size for floppy emulated boot image."));
			return false;
		}

		// sec_count = 1, only load one sector for floppies.
		m_BootImages.push_back(new ElToritoImage(
			szFullPath,bBootable,ElToritoImage::EMULATION_FLOPPY,0,1));
		return true;
	}

	bool ElTorito::AddBootImageHardDisk(const ckcore::tchar *szFullPath,bool bBootable)
	{
		if (m_BootImages.size() >= ELTORITO_MAX_BOOTIMAGE_COUNT)
			return false;

		if (!ckcore::File::Exist(szFullPath))
			return false;

		unsigned char ucDummy;
		if (!ReadSysTypeMBR(szFullPath,ucDummy))
			return false;

		// sec_count = 1, Only load the MBR.
		m_BootImages.push_back(new ElToritoImage(
			szFullPath,bBootable,ElToritoImage::EMULATION_HARDDISK,0,1));
		return true;
	}

	bool ElTorito::CalculateFileSysData(ckcore::tuint64 uiStartSec,
		ckcore::tuint64 &uiLastSec)
	{
		std::vector<ElToritoImage *>::iterator itImage;
		for (itImage = m_BootImages.begin(); itImage != m_BootImages.end(); itImage++)
		{
			if (uiStartSec > 0xFFFFFFFF)
			{
#ifdef _WINDOWS
				log_.PrintLine(ckT("  Error: Sector offset overflow (%I64u), can not include boot image: %s."),
#else
				log_.PrintLine(ckT("  Error: Sector offset overflow (%lld), can not include boot image: %s."),
#endif
					uiStartSec,(*itImage)->m_FullPath.c_str());
				return false;
			}

			(*itImage)->m_ulDataSecPos = (unsigned long)uiStartSec;

			uiStartSec += BytesToSector64((ckcore::tuint64)
				ckcore::File::Size((*itImage)->m_FullPath.c_str()));
		}

		uiLastSec = uiStartSec;
		return true;
	}

	ckcore::tuint64 ElTorito::GetBootCatSize()
	{
		// The validator and default boot image allocates 64 bytes, the remaining
		// boot images allocates 64 bytes a piece.
		return m_BootImages.size() << 6;
	}

	ckcore::tuint64 ElTorito::GetBootDataSize()
	{
		ckcore::tuint64 uiSize = 0;
		std::vector<ElToritoImage *>::iterator itImage;
		for (itImage = m_BootImages.begin(); itImage != m_BootImages.end(); itImage++)
			uiSize += BytesToSector64((ckcore::tuint64)ckcore::File::Size((*itImage)->m_FullPath.c_str()));

		return uiSize;
	}

	ckcore::tuint64 ElTorito::GetBootImageCount()
	{
		return m_BootImages.size();
	}
};
