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

#include <string.h>
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
		for (itImage = boot_images_.begin(); itImage != boot_images_.end(); itImage++)
			delete *itImage;

		boot_images_.clear();
	}

	bool ElTorito::ReadSysTypeMbr(const ckcore::tchar *full_path,unsigned char &sys_type)
	{
		// Find the system type in the path table located in the MBR.
		ckcore::FileInStream in_file(full_path);
		if (!in_file.Open())
		{
			log_.PrintLine(ckT("  Error: Unable to obtain file handle to \"%s\"."),full_path);
			return false;
		}

		teltorito_mbr mbr;

		ckcore::tint64 processed = in_file.Read(&mbr,sizeof(teltorito_mbr));
		if (processed == -1)
			return false;
		if (processed != sizeof(teltorito_mbr) ||
			(mbr.signature1 != 0x55 || mbr.signature2 != 0xAA))
		{
			log_.PrintLine(ckT("  Error: Unable to locate MBR in default boot image."));
			return false;
		}

		size_t used_partition = -1;
		for (size_t i = 0; i < MBR_PARTITION_COUNT; i++)
		{
			// Look for the first used partition.
			if (mbr.partitions[i].part_type != 0)
			{
				if (used_partition != -1)
				{
					log_.PrintLine(ckT("  Error: Invalid boot image, it contains more than one partition."));
					return false;
				}
				else
				{
					used_partition = i;
				}
			}
		}

		sys_type = mbr.partitions[used_partition].part_type;
		return true;
	}

	bool ElTorito::WriteBootRecord(SectorOutStream &out_stream,unsigned long boot_cat_sec_pos)
	{
		tiso_voldesc_eltorito_record br;
		memset(&br,0,sizeof(tiso_voldesc_eltorito_record));

		br.type = 0;
		memcpy(br.ident,iso_ident_cd,sizeof(br.ident));
		br.version = 1;
		memcpy(br.boot_sys_ident,iso_ident_eltorito,strlen(iso_ident_eltorito));
		br.boot_cat_ptr = boot_cat_sec_pos;

		ckcore::tint64 processed = out_stream.Write(&br,sizeof(br));
		if (processed == -1)
			return false;
		if (processed != sizeof(br))
			return false;

		return true;
	}

	bool ElTorito::WriteBootCatalog(SectorOutStream &out_stream)
	{
		char szManufacturer[] = { 0x49,0x4E,0x46,0x52,0x41,0x52,0x45,0x43,0x4F,0x52,0x44,0x45,0x52 };
		teltorito_valientry ve;
		memset(&ve,0,sizeof(teltorito_valientry));

		ve.header = 0x01;
		ve.platform = ELTORITO_PLATFORM_80X86;
		memcpy(ve.manufacturer,szManufacturer,13);
		ve.key_byte1 = 0x55;
		ve.key_byte2 = 0xAA;

		// Calculate check sum.
		int checksum = 0;
		unsigned char *pEntryPtr = (unsigned char *)&ve;
		for (size_t i = 0; i < sizeof(teltorito_valientry); i += 2) {
			checksum += (unsigned int)pEntryPtr[i];
			checksum += ((unsigned int)pEntryPtr[i + 1]) << 8;
		}

		ve.checksum = -checksum;

		ckcore::tint64 processed = out_stream.Write(&ve,sizeof(ve));
		if (processed == -1)
			return false;
		if (processed != sizeof(ve))
			return false;

		// Write the default boot entry.
		if (boot_images_.size() < 1)
			return false;

		ElToritoImage *pDefImage = boot_images_[0];

		teltorito_defentry dbe;
		memset(&dbe,0,sizeof(teltorito_defentry));

		dbe.boot_indicator = pDefImage->bootable_ ?
			ELTORITO_BOOTINDICATOR_BOOTABLE : ELTORITO_BOOTINDICATOR_NONBOOTABLE;
		dbe.load_segment = pDefImage->load_segment_;
		dbe.sec_count = pDefImage->sec_count_;
		dbe.load_sec_addr = pDefImage->data_sec_pos_;

		switch (pDefImage->emulation_)
		{
			case ElToritoImage::EMULATION_NONE:
				dbe.emulation = ELTORITO_EMULATION_NONE;
				dbe.sys_type = 0;
				break;

			case ElToritoImage::EMULATION_FLOPPY:
				switch (ckcore::File::Size(pDefImage->full_path_.c_str()))
				{
					case 1200 * 1024:
						dbe.emulation = ELTORITO_EMULATION_DISKETTE12;
						break;
					case 1440 * 1024:
						dbe.emulation = ELTORITO_EMULATION_DISKETTE144;
						break;
					case 2880 * 1024:
						dbe.emulation = ELTORITO_EMULATION_DISKETTE288;
						break;
					default:
						return false;
				}

				dbe.sys_type = 0;
				break;

			case ElToritoImage::EMULATION_HARDDISK:
				dbe.emulation = ELTORITO_EMULATION_HARDDISK;

				if (!ReadSysTypeMbr(pDefImage->full_path_.c_str(),dbe.sys_type))
					return false;
				break;
		}

		processed = out_stream.Write(&dbe,sizeof(dbe));
		if (processed == -1)
			return false;
		if (processed != sizeof(dbe))
			return false;

		// Write the rest of the boot images.
		teltorito_sec_header sh;
		teltorito_sec_entry se;

		unsigned short usNumImages = (unsigned short)boot_images_.size();
		for (unsigned short i = 1; i < usNumImages; i++)
		{
			ElToritoImage *pCurImage = boot_images_[i];

			// Write section header.
			memset(&sh,0,sizeof(teltorito_sec_header));

			sh.header = i == (usNumImages - 1) ?
				ELTORITO_HEADER_FINAL : ELTORITO_HEADER_NORMAL;
			sh.platform = ELTORITO_PLATFORM_80X86;
			sh.num_sec_entries = 1;

			char szIdentifier[16];
			sprintf(szIdentifier,"IMAGE%u",i + 1);
			memcpy(sh.ident,szIdentifier,strlen(szIdentifier));

			processed = out_stream.Write(&sh,sizeof(sh));
			if (processed == -1)
				return false;
			if (processed != sizeof(sh))
				return false;

			// Write the section entry.
			memset(&se,0,sizeof(teltorito_sec_entry));

			se.boot_indicator = pCurImage->bootable_ ?
				ELTORITO_BOOTINDICATOR_BOOTABLE : ELTORITO_BOOTINDICATOR_NONBOOTABLE;
			se.load_segment = pCurImage->load_segment_;
			se.sec_count = pCurImage->sec_count_;
			se.load_sec_addr = pCurImage->data_sec_pos_;

			switch (pCurImage->emulation_)
			{
				case ElToritoImage::EMULATION_NONE:
					se.emulation = ELTORITO_EMULATION_NONE;
					se.sys_type = 0;
					break;

				case ElToritoImage::EMULATION_FLOPPY:
					switch (ckcore::File::Size(pCurImage->full_path_.c_str()))
					{
						case 1200 * 1024:
							se.emulation = ELTORITO_EMULATION_DISKETTE12;
							break;
						case 1440 * 1024:
							se.emulation = ELTORITO_EMULATION_DISKETTE144;
							break;
						case 2880 * 1024:
							se.emulation = ELTORITO_EMULATION_DISKETTE288;
							break;
						default:
							return false;
					}

					se.sys_type = 0;
					break;

				case ElToritoImage::EMULATION_HARDDISK:
					se.emulation = ELTORITO_EMULATION_HARDDISK;

					if (!ReadSysTypeMbr(pCurImage->full_path_.c_str(),se.sys_type))
						return false;
					break;
			}

			processed = out_stream.Write(&se,sizeof(se));
			if (processed == -1)
				return false;
			if (processed != sizeof(se))
				return false;
		}

		if (out_stream.GetAllocated() != 0)
			out_stream.PadSector();

		return true;
	}

	bool ElTorito::WriteBootImage(SectorOutStream &out_stream,const ckcore::tchar *full_path)
	{
		ckcore::FileInStream FileStream(full_path);
		if (!FileStream.Open())
		{
			log_.PrintLine(ckT("  Error: Unable to obtain file handle to \"%s\"."),full_path);
			return false;
		}

		char szBuffer[ELTORITO_IO_BUFFER_SIZE];

		while (!FileStream.End())
		{
			ckcore::tint64 processed = FileStream.Read(szBuffer,ELTORITO_IO_BUFFER_SIZE);
			if (processed == -1)
			{
				log_.PrintLine(ckT("  Error: Unable read file: %s."),full_path);
				return false;
			}

			if (out_stream.Write(szBuffer,(ckcore::tuint32)processed) == -1)
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
		std::vector<ElToritoImage *>::iterator it;
		for (it = boot_images_.begin(); it != boot_images_.end(); it++)
		{
			if (!WriteBootImage(out_stream,(*it)->full_path_.c_str()))
				return false;
		}

		return true;
	}

	bool ElTorito::AddBootImageNoEmu(const ckcore::tchar *full_path,bool bootable,
									 unsigned short load_segment,unsigned short sec_count)
	{
		if (boot_images_.size() >= ELTORITO_MAX_BOOTIMAGE_COUNT)
			return false;

		if (!ckcore::File::Exist(full_path))
			return false;

		boot_images_.push_back(new ElToritoImage(
			full_path,bootable,ElToritoImage::EMULATION_NONE,load_segment,sec_count));
		return true;
	}

	bool ElTorito::AddBootImageFloppy(const ckcore::tchar *full_path,bool bootable)
	{
		if (boot_images_.size() >= ELTORITO_MAX_BOOTIMAGE_COUNT)
			return false;

		if (!ckcore::File::Exist(full_path))
			return false;

		ckcore::tuint64 file_size = ckcore::File::Size(full_path);
		if (file_size != 1200 * 1024 && file_size != 1440 * 1024 && file_size != 2880 * 1024)
		{
			log_.PrintLine(ckT("  Error: Invalid file size for floppy emulated boot image."));
			return false;
		}

		// sec_count = 1, only load one sector for floppies.
		boot_images_.push_back(new ElToritoImage(
			full_path,bootable,ElToritoImage::EMULATION_FLOPPY,0,1));
		return true;
	}

	bool ElTorito::AddBootImageHardDisk(const ckcore::tchar *full_path,bool bootable)
	{
		if (boot_images_.size() >= ELTORITO_MAX_BOOTIMAGE_COUNT)
			return false;

		if (!ckcore::File::Exist(full_path))
			return false;

		unsigned char dummy;
		if (!ReadSysTypeMbr(full_path,dummy))
			return false;

		// sec_count = 1, Only load the MBR.
		boot_images_.push_back(new ElToritoImage(
			full_path,bootable,ElToritoImage::EMULATION_HARDDISK,0,1));
		return true;
	}

	bool ElTorito::CalculateFileSysData(ckcore::tuint64 start_sec,
										ckcore::tuint64 &last_sec)
	{
		std::vector<ElToritoImage *>::iterator it;
		for (it = boot_images_.begin(); it != boot_images_.end(); it++)
		{
			if (start_sec > 0xFFFFFFFF)
			{
#ifdef _WINDOWS
				log_.PrintLine(ckT("  Error: Sector offset overflow (%I64u), can not include boot image: %s."),
#else
				log_.PrintLine(ckT("  Error: Sector offset overflow (%lld), can not include boot image: %s."),
#endif
				start_sec,(*it)->full_path_.c_str());
				return false;
			}

			(*it)->data_sec_pos_ = (unsigned long)start_sec;

			start_sec += bytes_to_sec64((ckcore::tuint64)
				ckcore::File::Size((*it)->full_path_.c_str()));
		}

		last_sec = start_sec;
		return true;
	}

	ckcore::tuint64 ElTorito::GetBootCatSize()
	{
		// The validator and default boot image allocates 64 bytes, the remaining
		// boot images allocates 64 bytes a piece.
		return boot_images_.size() << 6;
	}

	ckcore::tuint64 ElTorito::GetBootDataSize()
	{
		ckcore::tuint64 size = 0;
		std::vector<ElToritoImage *>::iterator it;
		for (it = boot_images_.begin(); it != boot_images_.end(); it++)
			size += bytes_to_sec64((ckcore::tuint64)ckcore::File::Size((*it)->full_path_.c_str()));

		return size;
	}

	ckcore::tuint64 ElTorito::GetBootImageCount()
	{
		return boot_images_.size();
	}
};
