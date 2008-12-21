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
#include <string.h>
#include "ckfilesystem/iso9660.hh"

namespace ckfilesystem
{
	/*
		Global Identifiers.
	*/
	const char *iso_ident_cd = "CD001";
	const char *iso_ident_eltorito = "EL TORITO SPECIFICATION";

	/*
		Helper Functions.
	*/
	void write721(unsigned char *buffer,unsigned short val)		// Least significant byte first.
	{
		buffer[0] = val & 0xFF;
		buffer[1] = (val >> 8) & 0xFF;
	}

	void write722(unsigned char *buffer,unsigned short val)		// Most significant byte first.
	{
		buffer[0] = (val >> 8) & 0xFF;
		buffer[1] = val & 0xFF;
	}

	void write723(unsigned char *buffer,unsigned short val)		// Both-byte orders.
	{
		buffer[3] = buffer[0] = val & 0xFF;
		buffer[2] = buffer[1] = (val >> 8) & 0xFF;
	}

	void write731(unsigned char *buffer,unsigned long val)		// Least significant byte first.
	{
		buffer[0] = (unsigned char)(val & 0xFF);
		buffer[1] = (unsigned char)((val >> 8) & 0xFF);
		buffer[2] = (unsigned char)((val >> 16) & 0xFF);
		buffer[3] = (unsigned char)((val >> 24) & 0xFF);
	}

	void write732(unsigned char *buffer,unsigned long val)		// Most significant byte first.
	{
		buffer[0] = (unsigned char)((val >> 24) & 0xFF);
		buffer[1] = (unsigned char)((val >> 16) & 0xFF);
		buffer[2] = (unsigned char)((val >> 8) & 0xFF);
		buffer[3] = (unsigned char)(val & 0xFF);
	}

	void write733(unsigned char *buffer,unsigned long val)		// Both-byte orders.
	{
		buffer[7] = buffer[0] = (unsigned char)(val & 0xFF);
		buffer[6] = buffer[1] = (unsigned char)((val >> 8) & 0xFF);
		buffer[5] = buffer[2] = (unsigned char)((val >> 16) & 0xFF);
		buffer[4] = buffer[3] = (unsigned char)((val >> 24) & 0xFF);
	}

	void write72(unsigned char *buffer,unsigned short val,bool msbf)
	{
		if (msbf)
			write722(buffer,val);
		else
			write721(buffer,val);
	}

	void write73(unsigned char *buffer,unsigned long val,bool msbf)
	{
		if (msbf)
			write732(buffer,val);
		else
			write731(buffer,val);
	}

	unsigned short read721(unsigned char *buffer)		// Least significant byte first.
	{
		return ((unsigned short)buffer[1] << 8) | buffer[0];
	}

	unsigned short read722(unsigned char *buffer)		// Most significant byte first.
	{
		return ((unsigned short)buffer[0] << 8) | buffer[1];
	}

	unsigned short read723(unsigned char *buffer)		// Both-byte orders.
	{
		return read721(buffer);
	}

	unsigned long read731(unsigned char *buffer)			// Least significant byte first.
	{
		return ((unsigned long)buffer[3] << 24) | ((unsigned long)buffer[2] << 16) |
			((unsigned long)buffer[1] << 8) | buffer[0];
	}

	unsigned long read732(unsigned char *buffer)			// Most significant byte first.
	{
		return ((unsigned long)buffer[0] << 24) | ((unsigned long)buffer[1] << 16) |
			((unsigned long)buffer[2] << 8) | buffer[3];
	}

	unsigned long read733(unsigned char *buffer)			// Both-byte orders.
	{
		return read731(buffer);
	}

	unsigned long bytes_to_sec(unsigned long bytes)
	{
		if (bytes == 0)
			return 0;

		unsigned long sectors = 1;
		while (bytes > ISO9660_SECTOR_SIZE)
		{
			bytes -= ISO9660_SECTOR_SIZE;
			sectors++;
		}

		return sectors;
	}

	unsigned long bytes_to_sec(ckcore::tuint64 bytes)
	{
		if (bytes == 0)
			return 0;

		unsigned long sectors = 1;
		while (bytes > ISO9660_SECTOR_SIZE)
		{
			bytes -= ISO9660_SECTOR_SIZE;
			sectors++;
		}

		return sectors;
	}

	ckcore::tuint64 bytes_to_sec64(ckcore::tuint64 bytes)
	{
		if (bytes == 0)
			return 0;

		ckcore::tuint64 sectors = 1;
		while (bytes > ISO9660_SECTOR_SIZE)
		{
			bytes -= ISO9660_SECTOR_SIZE;
			sectors++;
		}

		return sectors;
	}

	void iso_make_datetime(struct tm &time,tiso_voldesc_datetime &iso_time)
	{
		char buffer[5];
		sprintf(buffer,"%.4u",time.tm_year + 1900);
		memcpy(&iso_time.year,buffer,4);

		sprintf(buffer,"%.2u",time.tm_mon + 1);
		memcpy(&iso_time.mon,buffer,2);

		sprintf(buffer,"%.2u",time.tm_mday);
		memcpy(&iso_time.day,buffer,2);

		sprintf(buffer,"%.2u",time.tm_hour);
		memcpy(&iso_time.hour,buffer,2);

		sprintf(buffer,"%.2u",time.tm_min);
		memcpy(&iso_time.min,buffer,2);

        if (time.tm_sec > 59)
            sprintf(buffer,"%.2u",59);
        else
            sprintf(buffer,"%.2u",time.tm_sec);
		memcpy(&iso_time.sec,buffer,2);

		sprintf(buffer,"%.2u",0);
		memcpy(&iso_time.hundreds,buffer,2);

#ifdef _WINDOWS
		TIME_ZONE_INFORMATION tzi;
		GetTimeZoneInformation(&tzi);
		iso_time.zone = -(unsigned char)(tzi.Bias/15);
#else
        // FIXME: Add support for Unix time zone information.
        iso_time.zone = 0;
#endif
	}

	void iso_make_datetime(struct tm &time,tiso_dir_record_datetime &iso_time)
	{
		iso_time.year = (unsigned char)time.tm_year;
		iso_time.mon = (unsigned char)time.tm_mon + 1;
		iso_time.day = (unsigned char)time.tm_mday;
		iso_time.hour = (unsigned char)time.tm_hour;
		iso_time.min = (unsigned char)time.tm_min;
        iso_time.sec = time.tm_sec > 59 ? 59 : (unsigned char)time.tm_sec;

#ifdef _WINDOWS
		TIME_ZONE_INFORMATION tzi;
		GetTimeZoneInformation(&tzi);
		iso_time.zone = -(unsigned char)(tzi.Bias/15);
#else
        // FIXME: Add support for UNIX time zone information.
        iso_time.zone = 0;
#endif
	}

	void iso_make_datetime(unsigned short date,unsigned short time,
						   tiso_dir_record_datetime &iso_time)
	{
		iso_time.year = ((date >> 9) & 0x7F) + 80;
		iso_time.mon = (date >> 5) & 0x0F;
		iso_time.day = date & 0x1F;
		iso_time.hour = (time >> 11) & 0x1F;
		iso_time.min = (time >> 5) & 0x3F;
		iso_time.sec = (time & 0x1F) << 1;

#ifdef _WINDOWS
		TIME_ZONE_INFORMATION tzi;
		GetTimeZoneInformation(&tzi);
		iso_time.zone = -(unsigned char)(tzi.Bias/15);
#else
        // FIXME: Add support for Unix time zone information.
        iso_time.zone = 0;
#endif
	}

	void iso_make_dosdatetime(tiso_dir_record_datetime &iso_time,unsigned short &date,unsigned short &time)
	{
		date = ((iso_time.year - 80) & 0x7F) << 9;
		date |= (iso_time.mon & 0x7F) << 5;
		date |= (iso_time.day & 0x1F);

		time = (iso_time.hour & 0x1F) << 11;
		time |= (iso_time.min & 0x3F) << 5;
		time |= (iso_time.sec & 0x1F) >> 1;
	}

	Iso9660::Iso9660() : relax_max_dir_level_(false),inc_file_ver_info_(true),
		inter_level_(LEVEL_1)
	{
		init_vol_desc_primary();
		init_vol_desc_setterm();
	}

	Iso9660::~Iso9660()
	{
	}

	/*
		Convert the specified character to an a-character (appendix A).
	*/
	char Iso9660::make_char_a(char c)
	{
		char res = toupper(c);

		// Make sure that it's a valid character, otherwise return '_'.
		if ((res >= 0x20 && res <= 0x22) ||
			(res >= 0x25 && res <= 0x39) ||
			(res >= 0x41 && res <= 0x5A) || res == 0x5F)
			return res;

		return '_';
	}

	/*
		Convert the specified character to a d-character (appendix A).
	*/
	char Iso9660::make_char_d(char c)
	{
		char res = toupper(c);

		// Make sure that it's a valid character, otherwise return '_'.
		if ((res >= 0x30 && res <= 0x39) ||
			(res >= 0x41 && res <= 0x5A) || res == 0x5F)
			return res;

		return '_';
	}

    /*
     * Fins the last specified delimiter in the specified string.
     */
    int Iso9660::last_delimiter_a(const char *str,char delim)
    {    
        int len = (int)strlen(str);
        for (int i = len - 1; i >= 0; i--)
        {
            if (str[i] == delim)
                return i;
        }

        return -1;
    }

	/*
		Performs a memory copy from source to target, all characters
		in target will be A-characters.
	*/
	void Iso9660::mem_str_cpy_a(unsigned char *target,const char *source,size_t len)
	{
		for (size_t i = 0; i < len; i++)
			target[i] = make_char_a(source[i]);
	}

	/*
		Performs a memory copy from source to target, all characters
		in target will be D-characters.
	*/
	void Iso9660::mem_str_cpy_d(unsigned char *target,const char *source,size_t len)
	{
		for (size_t i = 0; i < len; i++)
			target[i] = make_char_d(source[i]);
	}

	/*
		Converts the input file name to a valid ISO level 1 file name. This means:
		 - A maximum of 12 characters.
		 - A file extension of at most 3 characters.
		 - A file name of at most 8 characters.
	*/
	unsigned char Iso9660::write_file_name_l1(unsigned char *buffer,const ckcore::tchar *file_name)
	{
		int file_name_len = (int)ckcore::string::astrlen(file_name);
		unsigned char len = 0;

		char *ansi_file_name;
	#ifdef _UNICODE
		ansi_file_name = new char [file_name_len + 1];
		ckcore::string::utf16_to_ansi(file_name,ansi_file_name,file_name_len + 1);
	#else
		ansi_file_name = (char *)file_name;
	#endif

		int ext_delim = last_delimiter_a(ansi_file_name,'.');
		if (ext_delim == -1)
		{
			size_t max = file_name_len < 8 ? file_name_len : 8;
			for (size_t i = 0; i < max; i++)
				buffer[i] = make_char_d(ansi_file_name[i]);
			
			len = (unsigned char)max;
			buffer[max] = '\0';
		}
		else
		{
			int ext_len = (int)file_name_len - ext_delim - 1;
			if (ext_len > 3)	// Level one support a maxmimum file extension of length 3.
				ext_len = 3;

			size_t max = ext_delim < 8 ? ext_delim : 8;
			for (size_t i = 0; i < max; i++)
				buffer[i] = make_char_d(ansi_file_name[i]);

			buffer[max] = '.';

			// Copy the extension.
			for (size_t i = max + 1; i < max + ext_len + 1; i++)
				buffer[i] = make_char_d(ansi_file_name[++ext_delim]);

			len = (unsigned char)max + (unsigned char)ext_len + 1;
			buffer[len] = '\0';
		}

	#ifdef _UNICODE
		delete [] ansi_file_name;
	#endif

		return len;
	}

	unsigned char Iso9660::write_file_name_generic(unsigned char *buffer,const ckcore::tchar *file_name,
												int max_len)
	{
		int file_name_len = (int)ckcore::string::astrlen(file_name);
		unsigned char len = 0;

	#ifdef _UNICODE
		char *ansi_file_name = new char [file_name_len + 1];
		ckcore::string::utf16_to_ansi(file_name,ansi_file_name,file_name_len + 1);
	#else
		char *ansi_file_name = (char *)file_name;
	#endif

		int ext_delim = last_delimiter_a(ansi_file_name,'.');
		if (ext_delim == -1)
		{
			size_t max = file_name_len < max_len ? file_name_len : max_len;
			for (size_t i = 0; i < max; i++)
				buffer[i] = make_char_d(ansi_file_name[i]);
			
			len = (unsigned char)max;
			buffer[max] = '\0';
		}
		else
		{
			int ext_len = (int)file_name_len - ext_delim - 1;
			if (ext_len > max_len - 1)	// The file can at most contain an extension of length max_len - 1 characters.
				ext_len = max_len - 1;

			size_t max = ext_delim < (max_len - ext_len) ? ext_delim : (max_len - 1 - ext_len);
			for (size_t i = 0; i < max; i++)
				buffer[i] = make_char_d(ansi_file_name[i]);

			buffer[max] = '.';

			// Copy the extension.
			for (size_t i = max + 1; i < max + ext_len + 1; i++)
				buffer[i] = make_char_d(ansi_file_name[++ext_delim]);

			len = (unsigned char)max + (unsigned char)ext_len + 1;
			buffer[len] = '\0';
		}

	#ifdef _UNICODE
		delete [] ansi_file_name;
	#endif

		return len;
	}

	/*
		Converts the input file name to a valid ISO level 2 and above file name. This means:
		 - A maximum of 31 characters.
	*/
	unsigned char Iso9660::write_file_name_l2(unsigned char *buffer,const ckcore::tchar *file_name)
	{
		return write_file_name_generic(buffer,file_name,31);
	}

	unsigned char Iso9660::write_file_name_1999(unsigned char *buffer,const ckcore::tchar *file_name)
	{
		return write_file_name_generic(buffer,file_name,ISO9660_MAX_NAMELEN_1999);
	}

	unsigned char Iso9660::write_dir_name_l1(unsigned char *buffer,const ckcore::tchar *dir_name)
	{
		int dir_name_len = (int)ckcore::string::astrlen(dir_name);
		int max = dir_name_len < 8 ? dir_name_len : 8;

	#ifdef _UNICODE
		char *ansi_dir_name = new char [dir_name_len + 1];
		ckcore::string::utf16_to_ansi(dir_name,ansi_dir_name,dir_name_len + 1);
	#else
		char *ansi_dir_name = (char *)dir_name;
	#endif

		for (size_t i = 0; i < (size_t)max; i++)
			buffer[i] = make_char_d(ansi_dir_name[i]);
			
		buffer[max] = '\0';

	#ifdef _UNICODE
		delete [] ansi_dir_name;
	#endif

		return max;
	}

	unsigned char Iso9660::write_dir_name_generic(unsigned char *buffer,const ckcore::tchar *dir_name,
											   int max_len)
	{
		int dir_name_len = (int)ckcore::string::astrlen(dir_name);
		int max = dir_name_len < max_len ? dir_name_len : max_len;

	#ifdef _UNICODE
		char *ansi_dir_name = new char [dir_name_len + 1];
		ckcore::string::utf16_to_ansi(dir_name,ansi_dir_name,dir_name_len + 1);
	#else
		char *ansi_dir_name = (char *)dir_name;
	#endif

		for (size_t i = 0; i < (size_t)max; i++)
			buffer[i] = make_char_d(ansi_dir_name[i]);
			
		buffer[max] = '\0';

	#ifdef _UNICODE
		delete [] ansi_dir_name;
	#endif

		return max;
	}

	unsigned char Iso9660::write_dir_name_l2(unsigned char *buffer,const ckcore::tchar *dir_name)
	{
		return write_dir_name_generic(buffer,dir_name,31);
	}

	unsigned char Iso9660::write_dir_name_1999(unsigned char *buffer,const ckcore::tchar *dir_name)
	{
		return write_dir_name_generic(buffer,dir_name,ISO9660_MAX_NAMELEN_1999);
	}

	unsigned char Iso9660::calc_file_name_len_l1(const ckcore::tchar *file_name)
	{
		unsigned char szTempBuffer[13];
		return write_file_name_l1(szTempBuffer,file_name);
	}

	unsigned char Iso9660::calc_file_name_len_l2(const ckcore::tchar *file_name)
	{
		size_t file_name_len = ckcore::string::astrlen(file_name);
		if (file_name_len < 31)
			return (unsigned char)file_name_len;

		return 31;
	}

	unsigned char Iso9660::calc_file_name_len_1999(const ckcore::tchar *file_name)
	{
		size_t file_name_len = ckcore::string::astrlen(file_name);
		if (file_name_len < ISO9660_MAX_NAMELEN_1999)
			return (unsigned char)file_name_len;

		return ISO9660_MAX_NAMELEN_1999;
	}

	unsigned char Iso9660::calc_dir_name_len_l1(const ckcore::tchar *dir_name)
	{
		size_t dir_name_len = ckcore::string::astrlen(dir_name);
		if (dir_name_len < 8)
			return (unsigned char)dir_name_len;

		return 8;
	}

	unsigned char Iso9660::calc_dir_name_len_l2(const ckcore::tchar *dir_name)
	{
		size_t dir_name_len = ckcore::string::astrlen(dir_name);
		if (dir_name_len < 31)
			return (unsigned char)dir_name_len;

		return 31;
	}

	unsigned char Iso9660::calc_dir_name_len_1999(const ckcore::tchar *dir_name)
	{
		size_t dir_name_len = ckcore::string::astrlen(dir_name);
		if (dir_name_len < ISO9660_MAX_NAMELEN_1999)
			return (unsigned char)dir_name_len;

		return ISO9660_MAX_NAMELEN_1999;
	}

	void Iso9660::init_vol_desc_primary()
	{
		// Clear memory.
		memset(&voldesc_primary_,0,sizeof(voldesc_primary_));
		memset(voldesc_primary_.sys_ident,0x20,sizeof(voldesc_primary_.sys_ident));
		memset(voldesc_primary_.vol_ident,0x20,sizeof(voldesc_primary_.vol_ident));
		memset(voldesc_primary_.volset_ident,0x20,sizeof(voldesc_primary_.volset_ident));
		memset(voldesc_primary_.publ_ident,0x20,sizeof(voldesc_primary_.publ_ident));
		memset(voldesc_primary_.prep_ident,0x20,sizeof(voldesc_primary_.prep_ident));
		memset(voldesc_primary_.app_ident,0x20,sizeof(voldesc_primary_.app_ident));
		memset(voldesc_primary_.copy_file_ident,0x20,sizeof(voldesc_primary_.copy_file_ident));
		memset(voldesc_primary_.abst_file_ident,0x20,sizeof(voldesc_primary_.abst_file_ident));
		memset(voldesc_primary_.bibl_file_ident,0x20,sizeof(voldesc_primary_.bibl_file_ident));

		// Set primary volume descriptor header.
		voldesc_primary_.type = VOLDESCTYPE_PRIM_VOL_DESC;
		voldesc_primary_.version = 1;
		voldesc_primary_.file_struct_ver = 1;
		memcpy(voldesc_primary_.ident,iso_ident_cd,sizeof(voldesc_primary_.ident));	

		// Set the root directory record.
		voldesc_primary_.root_dir_record.dir_record_len = 34;
		voldesc_primary_.root_dir_record.file_flags = DIRRECORD_FILEFLAG_DIRECTORY;
		voldesc_primary_.root_dir_record.file_ident_len = 1;	// One byte is always allocated in the tiso_dir_record structure.

		// Set application identifier.
		memset(voldesc_primary_.app_data,0x20,sizeof(voldesc_primary_.app_data));
		char app_ident[] = { 0x49,0x4E,0x46,0x52,0x41,0x52,0x45,0x43,0x4F,
			0x52,0x44,0x45,0x52,0x20,0x28,0x43,0x29,0x20,0x32,0x30,0x30,0x36,0x2D,
			0x32,0x30,0x30,0x38,0x20,0x43,0x48,0x52,0x49,0x53,0x54,0x49,0x41,0x4E,
			0x20,0x4B,0x49,0x4E,0x44,0x41,0x48,0x4C };
		memcpy(voldesc_primary_.app_ident,app_ident,45);
	}

	void Iso9660::init_vol_desc_setterm()
	{
		// Clear memory.
		memset(&voldesc_setterm_,0,sizeof(voldesc_setterm_));

		// Set volume descriptor set terminator header.
		voldesc_setterm_.type = VOLDESCTYPE_VOL_DESC_SET_TERM;
		voldesc_setterm_.version = 1;
		memcpy(voldesc_setterm_.ident,iso_ident_cd,sizeof(voldesc_setterm_.ident));
	}

	void Iso9660::set_volume_label(const ckcore::tchar *label)
	{
		size_t label_len = ckcore::string::astrlen(label);
		size_t label_copy_len = label_len < 32 ? label_len : 32;

		memset(voldesc_primary_.vol_ident,0x20,sizeof(voldesc_primary_.vol_ident));

	#ifdef _UNICODE
		char ansi_label[33];
		ckcore::string::utf16_to_ansi(label,ansi_label,sizeof(ansi_label));
		mem_str_cpy_d(voldesc_primary_.vol_ident,ansi_label,label_copy_len);
	#else
		mem_str_cpy_d(voldesc_primary_.vol_ident,label,label_copy_len);
	#endif
	}

	void Iso9660::set_text_fields(const ckcore::tchar *sys_ident,const ckcore::tchar *volset_ident,
								  const ckcore::tchar *publ_ident,const ckcore::tchar *prep_ident)
	{
		size_t sys_ident_len = ckcore::string::astrlen(sys_ident);
		size_t volset_ident_len = ckcore::string::astrlen(volset_ident);
		size_t publ_ident_len = ckcore::string::astrlen(publ_ident);
		size_t prep_ident_len = ckcore::string::astrlen(prep_ident);

		size_t sys_ident_copy_len = sys_ident_len < 32 ? sys_ident_len : 32;
		size_t volset_ident_copy_len = volset_ident_len < 128 ? volset_ident_len : 128;
		size_t publ_ident_copy_len = publ_ident_len < 128 ? publ_ident_len : 128;
		size_t prep_ident_copy_len = prep_ident_len < 128 ? prep_ident_len : 128;

		memset(voldesc_primary_.sys_ident,0x20,sizeof(voldesc_primary_.sys_ident));
		memset(voldesc_primary_.volset_ident,0x20,sizeof(voldesc_primary_.volset_ident));
		memset(voldesc_primary_.publ_ident,0x20,sizeof(voldesc_primary_.publ_ident));
		memset(voldesc_primary_.prep_ident,0x20,sizeof(voldesc_primary_.prep_ident));

	#ifdef _UNICODE
		char ansi_sys_ident[33];
		char ansi_volset_ident[129];
		char ansi_publ_ident[129];
		char ansi_prep_ident[129];

		ckcore::string::utf16_to_ansi(sys_ident,ansi_sys_ident,sizeof(ansi_sys_ident));
		ckcore::string::utf16_to_ansi(volset_ident,ansi_volset_ident,sizeof(ansi_volset_ident));
		ckcore::string::utf16_to_ansi(publ_ident,ansi_publ_ident,sizeof(ansi_publ_ident));
		ckcore::string::utf16_to_ansi(prep_ident,ansi_prep_ident,sizeof(ansi_prep_ident));

		mem_str_cpy_a(voldesc_primary_.sys_ident,ansi_sys_ident,sys_ident_copy_len);
		mem_str_cpy_d(voldesc_primary_.volset_ident,ansi_volset_ident,volset_ident_copy_len);
		mem_str_cpy_a(voldesc_primary_.publ_ident,ansi_publ_ident,publ_ident_copy_len);
		mem_str_cpy_a(voldesc_primary_.prep_ident,ansi_prep_ident,prep_ident_copy_len);
	#else
		mem_str_cpy_a(voldesc_primary_.sys_ident,sys_ident,sys_ident_copy_len);
		mem_str_cpy_d(voldesc_primary_.volset_ident,volset_ident,volset_ident_copy_len);
		mem_str_cpy_a(voldesc_primary_.publ_ident,publ_ident,publ_ident_copy_len);
		mem_str_cpy_a(voldesc_primary_.prep_ident,prep_ident,prep_ident_copy_len);
	#endif
	}

	void Iso9660::set_file_fields(const ckcore::tchar *copy_file_ident,
								  const ckcore::tchar *abst_file_ident,
						    	  const ckcore::tchar *bibl_file_ident)
	{
		size_t copy_file_ident_len = ckcore::string::astrlen(copy_file_ident);
		size_t abst_file_ident_len = ckcore::string::astrlen(abst_file_ident);
		size_t bibl_file_ident_len = ckcore::string::astrlen(bibl_file_ident);

		size_t copy_file_ident_copy_len = copy_file_ident_len < 37 ? copy_file_ident_len : 37;
		size_t abst_file_ident_copy_len = abst_file_ident_len < 37 ? abst_file_ident_len : 37;
		size_t bibl_file_ident_copy_len = bibl_file_ident_len < 37 ? bibl_file_ident_len : 37;

		memset(voldesc_primary_.copy_file_ident,0x20,sizeof(voldesc_primary_.copy_file_ident));
		memset(voldesc_primary_.abst_file_ident,0x20,sizeof(voldesc_primary_.abst_file_ident));
		memset(voldesc_primary_.bibl_file_ident,0x20,sizeof(voldesc_primary_.bibl_file_ident));

	#ifdef _UNICODE
		char ansi_copy_file_ident[38];
		char ansi_abst_file_ident[38];
		char ansi_bibl_file_ident[38];

		ckcore::string::utf16_to_ansi(copy_file_ident,ansi_copy_file_ident,sizeof(ansi_copy_file_ident));
		ckcore::string::utf16_to_ansi(abst_file_ident,ansi_abst_file_ident,sizeof(ansi_abst_file_ident));
		ckcore::string::utf16_to_ansi(bibl_file_ident,ansi_bibl_file_ident,sizeof(ansi_bibl_file_ident));

		mem_str_cpy_d(voldesc_primary_.copy_file_ident,ansi_copy_file_ident,copy_file_ident_copy_len);
		mem_str_cpy_d(voldesc_primary_.abst_file_ident,ansi_abst_file_ident,abst_file_ident_copy_len);
		mem_str_cpy_d(voldesc_primary_.bibl_file_ident,ansi_bibl_file_ident,bibl_file_ident_copy_len);
	#else
		mem_str_cpy_d(voldesc_primary_.copy_file_ident,copy_file_ident,copy_file_ident_copy_len);
		mem_str_cpy_d(voldesc_primary_.abst_file_ident,abst_file_ident,abst_file_ident_copy_len);
		mem_str_cpy_d(voldesc_primary_.bibl_file_ident,bibl_file_ident,bibl_file_ident_copy_len);
	#endif
	}

	void Iso9660::set_interchange_level(InterLevel inter_level)
	{
		inter_level_ = inter_level;
	}

	void Iso9660::set_relax_max_dir_level(bool relax)
	{
		relax_max_dir_level_ = relax;
	}

	void Iso9660::set_include_file_ver_info(bool include)
	{
		inc_file_ver_info_ = include;
	}

	bool Iso9660::write_vol_desc_primary(ckcore::OutStream &out_stream,struct tm &create_time,
									     unsigned long vol_space_size,unsigned long pathtable_size,
									     unsigned long pos_pathtable_l,unsigned long pos_pathtable_m,
									     unsigned long root_extent_loc,unsigned long data_len)
	{
		// Initialize the primary volume descriptor.
		write733(voldesc_primary_.vol_space_size,vol_space_size);		// Volume size in sectors.
		write723(voldesc_primary_.volset_size,1);						// Only one disc in the volume set.
		write723(voldesc_primary_.volseq_num,1);						// This is the first disc in the volume set.
		write723(voldesc_primary_.logical_block_size,ISO9660_SECTOR_SIZE);
		write733(voldesc_primary_.path_table_size,pathtable_size);		// Path table size in bytes.
		write731(voldesc_primary_.path_table_type_l,pos_pathtable_l);	// Start sector of LSBF path table.
		write732(voldesc_primary_.path_table_type_m,pos_pathtable_m);	// Start sector of MSBF path table.

		write733(voldesc_primary_.root_dir_record.extent_loc,root_extent_loc);
		write733(voldesc_primary_.root_dir_record.data_len,data_len);
		write723(voldesc_primary_.root_dir_record.volseq_num,1);	// The file extent is on the first volume set.

		// Time information.
		iso_make_datetime(create_time,voldesc_primary_.root_dir_record.rec_timestamp);

		iso_make_datetime(create_time,voldesc_primary_.create_time);
		memcpy(&voldesc_primary_.modify_time,&voldesc_primary_.create_time,sizeof(tiso_voldesc_datetime));

		memset(&voldesc_primary_.expr_time,'0',sizeof(tiso_voldesc_datetime));
		voldesc_primary_.expr_time.zone = 0x00;
		memset(&voldesc_primary_.effect_time,'0',sizeof(tiso_voldesc_datetime));
		voldesc_primary_.effect_time.zone = 0x00;

		// Write the primary volume descriptor.
		ckcore::tint64 processed = out_stream.write(&voldesc_primary_,sizeof(voldesc_primary_));
		if (processed == -1)
			return false;
		if (processed != sizeof(voldesc_primary_))
			return false;

		return true;
	}

	bool Iso9660::write_vol_desc_suppl(ckcore::OutStream &out_stream,struct tm &create_time,
								       unsigned long vol_space_size,unsigned long pathtable_size,
									   unsigned long pos_pathtable_l,unsigned long pos_pathtable_m,
									   unsigned long root_extent_loc,unsigned long data_len)
	{
		if (inter_level_ == ISO9660_1999)
		{
			tiso_voldesc_suppl sd;
			memcpy(&sd,&voldesc_primary_,sizeof(tiso_voldesc_suppl));

			// Update the version information.
			voldesc_primary_.type = VOLDESCTYPE_SUPPL_VOL_DESC;
			voldesc_primary_.version = 2;			// ISO9660:1999
			voldesc_primary_.file_struct_ver = 2;	// ISO9660:1999

			// Rewrite the values from the primary volume descriptor. We can't guarantee that
			// write_vol_desc_primary has been called before this function call, even though it
			// should have.
			write733(sd.vol_space_size,vol_space_size);		// Volume size in sectors.
			write723(sd.volset_size,1);		// Only one disc in the volume set.
			write723(sd.volseq_num,1);	// This is the first disc in the volume set.
			write723(sd.logical_block_size,ISO9660_SECTOR_SIZE);
			write733(sd.path_table_size,pathtable_size);	// Path table size in bytes.
			write731(sd.path_table_type_l,pos_pathtable_l);	// Start sector of LSBF path table.
			write732(sd.path_table_type_m,pos_pathtable_m);	// Start sector of MSBF path table.

			write733(sd.root_dir_record.extent_loc,root_extent_loc);
			write733(sd.root_dir_record.data_len,data_len);
			write723(sd.root_dir_record.volseq_num,1);	// The file extent is on the first volume set.

			// Time information.
			iso_make_datetime(create_time,sd.root_dir_record.rec_timestamp);

			iso_make_datetime(create_time,sd.create_time);
			memcpy(&sd.modify_time,&sd.create_time,sizeof(tiso_voldesc_datetime));

			memset(&sd.expr_time,'0',sizeof(tiso_voldesc_datetime));
			sd.expr_time.zone = 0x00;
			memset(&sd.effect_time,'0',sizeof(tiso_voldesc_datetime));
			sd.effect_time.zone = 0x00;

			// Write the primary volume descriptor.
			ckcore::tint64 processed = out_stream.write(&sd,sizeof(sd));
			if (processed == -1)
				return false;
			if (processed != sizeof(sd))
				return false;

			return true;
		}

		return false;
	}

	bool Iso9660::write_vol_desc_setterm(ckcore::OutStream &out_stream)
	{
		// Write volume descriptor set terminator.
		ckcore::tint64 processed = out_stream.write(&voldesc_setterm_,sizeof(voldesc_setterm_));
		if (processed == -1)
			return false;
		if (processed != sizeof(voldesc_setterm_))
			return false;

		return true;
	}

	unsigned char Iso9660::write_file_name(unsigned char *buffer,const ckcore::tchar *file_name,
										   bool is_dir)
	{
		switch (inter_level_)
		{
			case LEVEL_1:
			default:
				if (is_dir)
				{
					return write_dir_name_l1(buffer,file_name);
				}
				else
				{
					unsigned char file_name_len = write_file_name_l1(buffer,file_name);

					if (inc_file_ver_info_)
					{
						buffer[file_name_len++] = ';';
						buffer[file_name_len++] = '1';
					}

					return file_name_len;
				}
				break;

			case LEVEL_2:
				if (is_dir)
				{
					return write_dir_name_l2(buffer,file_name);
				}
				else
				{
					unsigned char file_name_len = write_file_name_l2(buffer,file_name);

					if (inc_file_ver_info_)
					{
						buffer[file_name_len++] = ';';
						buffer[file_name_len++] = '1';
					}

					return file_name_len;
				}
				break;

			case ISO9660_1999:
				if (is_dir)
					return write_dir_name_1999(buffer,file_name);
				else
					return write_file_name_1999(buffer,file_name);
		}
	}

	/**
		Determines the length of the compatible file name generated from the
		specified file name. A compatible file name is a file name that is
		generated from the specified file name that is supported by the current
		file system configuration.
		@param file_name the origial file name.
		@param is_dir if true, file_name is assumed to be a directory name.
		@return the length of the compatible file name.
	*/
	unsigned char Iso9660::calc_file_name_len(const ckcore::tchar *file_name,bool is_dir)
	{
		switch (inter_level_)
		{
			case LEVEL_1:
			default:
				if (is_dir)
				{
					return calc_dir_name_len_l1(file_name);
				}
				else
				{
					unsigned char file_name_len = calc_file_name_len_l1(file_name);

					if (inc_file_ver_info_)
						file_name_len += 2;

					return file_name_len;
				}
				break;

			case LEVEL_2:
				if (is_dir)
				{
					return calc_dir_name_len_l2(file_name);
				}
				else
				{
					unsigned char file_name_len = calc_file_name_len_l2(file_name);

					if (inc_file_ver_info_)
						file_name_len += 2;

					return file_name_len;
				}
				break;

			case ISO9660_1999:
				if (is_dir)
					return calc_dir_name_len_1999(file_name);
				else
					return calc_file_name_len_1999(file_name);
		}
	}

	/**
		Obtains the maximum numbe of directory levels supported by the current
		file system configuration.
		@return maximum number of allowed directory levels.
	*/
	unsigned char Iso9660::get_max_dir_level()
	{
		if (relax_max_dir_level_)
		{
			return ISO9660_MAX_DIRLEVEL_1999;
		}
		else
		{
			switch (inter_level_)
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
	bool Iso9660::has_vol_desc_suppl()
	{
		return inter_level_ == ISO9660_1999;
	}

	/**
		Checks whether the file system allows files to be fragmented (multiple
		extents) or not.
		@return true if the file system allows fragmentation.
	*/
	bool Iso9660::allows_fragmentation()
	{
		return inter_level_ == LEVEL_3;
	}

	/**
		Returns true if the file names includes the two character file version
		information (;1).
	*/
	bool Iso9660::includes_file_ver_info()
	{
		return inc_file_ver_info_;
	}
};
