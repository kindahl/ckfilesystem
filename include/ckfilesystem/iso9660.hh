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

// Fix to be able to include winbase.h.
#ifdef SetVolumeLabel
#undef SetVolumeLabel
#endif

#define ISO9660_SECTOR_SIZE						2048
#define ISO9660_MAX_NAMELEN_1999				 207
#define ISO9660_MAX_DIRLEVEL_NORMAL				   8		// Maximum is 8 for ISO9660:1988.
#define ISO9660_MAX_DIRLEVEL_1999				 255		// Maximum is 255 for ISO9660:1999.
#define ISO9660_MAX_EXTENT_SIZE					0xFFFFF800

#define DIRRECORD_FILEFLAG_HIDDEN				1 << 0
#define DIRRECORD_FILEFLAG_DIRECTORY			1 << 1
#define DIRRECORD_FILEFLAG_ASSOCIATEDFILE		1 << 2
#define DIRRECORD_FILEFLAG_RECORD				1 << 3
#define DIRRECORD_FILEFLAG_PROTECTION			1 << 4
#define DIRRECORD_FILEFLAG_MULTIEXTENT			1 << 7

#define VOLDESCTYPE_BOOT_CATALOG				0
#define VOLDESCTYPE_PRIM_VOL_DESC				1
#define VOLDESCTYPE_SUPPL_VOL_DESC				2
#define VOLDESCTYPE_VOL_PARTITION_DESC			3
#define VOLDESCTYPE_VOL_DESC_SET_TERM			255

namespace ckfilesystem
{
	/*
		Identifiers.
	*/
	extern const char *iso_ident_cd;
	extern const char *iso_ident_eltorito;

	/*
		File and Directory Descriptors.
	*/
#pragma pack(1)	// Force byte alignment.

	typedef struct
	{
		unsigned char year;		// Number of years since 1900.
		unsigned char mon;		// Month of the year from 1 to 12.
		unsigned char day;		// Day of the month from 1 to 31.
		unsigned char hour;		// Hour of the day from 0 to 23.
		unsigned char min;		// Minute of the hour from 0 to 59.
		unsigned char sec;		// Second of the minute from 0 to 59.
		unsigned char zone;		// Offset from Greenwich Mean Time in number of
								// 15 min intervals from -48 (West) to + 52 (East)
								// recorded according to 7.1.2.
	} tiso_dir_record_datetime;

	typedef struct
	{
		unsigned char dir_record_len;
		unsigned char ext_attr_record_len;
		unsigned char extent_loc[8];	// 7.3.3.
		unsigned char data_len[8];		// 7.3.3.
		tiso_dir_record_datetime rec_timestamp;
		unsigned char file_flags;
		unsigned char file_unit_size;
		unsigned char interleave_gap_size;
		unsigned char volseq_num[4];	// 7.2.3.
		unsigned char file_ident_len;
		unsigned char file_ident[1];	// Actually of size file_ident_len.
	} tiso_dir_record;

	typedef struct
	{
		unsigned char dir_ident_len;
		unsigned char ext_attr_record_len;
		unsigned char extent_loc[4];		// 7.3.?.
		unsigned char parent_dir_num[2];	// 7.2.?.
		unsigned char dir_ident[1];			// Actually consumes the rest of the
											// available path table record size.
	} tiso_pathtable_record;

	typedef struct
	{
		unsigned char owner_ident[4];		// 7.2.3.
		unsigned char group_ident[4];		// 7.2.3.
		unsigned short permissions;
		tiso_dir_record_datetime create_time;
		tiso_dir_record_datetime modify_time;
		tiso_dir_record_datetime expr_time;
		tiso_dir_record_datetime effect_time;
		unsigned char rec_format;
		unsigned char rec_attr;
		unsigned char rec_len[4];			// 7.2.3.
		unsigned char sys_ident[32];
		unsigned char sys_data[64];
		unsigned char ext_attr_record_ver;
		unsigned char esc_len;
		unsigned char res1[64];
		unsigned char app_data_len[4];		// 7.2.3.
		unsigned char app_data[1];			// Actually of size uiAppDataLen.
	} tiso_ext_attr_record;

	/*
		Volume Descriptors.
	*/
	typedef struct
	{
		unsigned int year;			// Year from I to 9999.
		unsigned short mon;			// Month of the year from 1 to 12.
		unsigned short day;			// Day of the month from 1 to 31.
		unsigned short hour;		// Hour of the day from 0 to 23.
		unsigned short min;			// Minute of the hour from 0 to 59.
		unsigned short sec;			// Second of the minute from 0 to 59.
		unsigned short hundreds;	// Hundredths of a second.
		unsigned char zone;			// Offset from Greenwich Mean Time in number of
									// 15 min intervals from -48 (West) to +52 (East)
									// recorded according to 7.1.2.
	} tiso_voldesc_datetime;

	typedef struct
	{
		unsigned char type;			// 0.
		unsigned char ident[5];		// "CD001".
		unsigned char version;
		unsigned char boot_sys_ident[32];
		unsigned char boot_ident[32];
		unsigned char boot_sys_data[1977];
	} tiso_voldesc_boot_record;		// Must be 2048 bytes in size.

	typedef struct
	{
		unsigned char type;					// 0.
		unsigned char ident[5];				// "CD001".
		unsigned char version;				// Must be 1.
		unsigned char boot_sys_ident[32];	// Must be "EL TORITO SPECIFICATION" padded with 0s.
		unsigned char unused1[32];			// Must be 0.
		unsigned int boot_cat_ptr;			// Absolute pointer to first sector of Boot Catalog.
		unsigned char boot_sys_data[1973];
	} tiso_voldesc_eltorito_record;	// Must be 2048 bytes in size.

	typedef struct
	{
		unsigned char type;
		unsigned char ident[5];					// "CD001".
		unsigned char version;
		unsigned char unused1;
		unsigned char sys_ident[32];
		unsigned char vol_ident[32];
		unsigned char unused2[8];
		unsigned char vol_space_size[8];		// 7.3.3.
		unsigned char unused3[32];
		unsigned char volset_size[4];			// 7.2.3.
		unsigned char volseq_num[4];			// 7.2.3.
		unsigned char logical_block_size[4];	// 7.2.3.
		unsigned char path_table_size[8];		// 7.3.3.
		unsigned char path_table_type_l[4];		// 7.3.1.
		unsigned char opt_path_table_type_l[4];	// 7.3.1.
		unsigned char path_table_type_m[4];		// 7.3.2.
		unsigned char opt_path_table_type_m[4];	// 7.3.2.
		tiso_dir_record root_dir_record;
		unsigned char volset_ident[128];
		unsigned char publ_ident[128];
		unsigned char prep_ident[128];
		unsigned char app_ident[128];
		unsigned char copy_file_ident[37];
		unsigned char abst_file_ident[37];
		unsigned char bibl_file_ident[37];
		tiso_voldesc_datetime create_time;
		tiso_voldesc_datetime modify_time;
		tiso_voldesc_datetime expr_time;
		tiso_voldesc_datetime effect_time;
		unsigned char file_struct_ver;
		unsigned char unused4;
		unsigned char app_data[512];
		unsigned char unused5[653];
	} tiso_voldesc_primary;		// Must be 2048 bytes in size.

	typedef struct
	{
		unsigned char type;
		unsigned char ident[5];					// "CD001".
		unsigned char version;
		unsigned char vol_flags;
		unsigned char sys_ident[32];
		unsigned char vol_ident[32];
		unsigned char unused1[8];
		unsigned char vol_space_size[8];		// 7.3.3.
		unsigned char esc_sec[32];
		unsigned char volset_size[4];			// 7.2.3.
		unsigned char volseq_num[4];			// 7.2.3.
		unsigned char logical_block_size[4];	// 7.2.3.
		unsigned char path_table_size[8];		// 7.3.3.
		unsigned char path_table_type_l[4];		// 7.3.1.
		unsigned char opt_path_table_type_l[4];	// 7.3.1.
		unsigned char path_table_type_m[4];		// 7.3.2.
		unsigned char opt_path_table_type_m[4];	// 7.3.2.
		tiso_dir_record root_dir_record;
		unsigned char volset_ident[128];
		unsigned char publ_ident[128];
		unsigned char prep_ident[128];
		unsigned char app_ident[128];
		unsigned char copy_file_ident[37];
		unsigned char abst_file_ident[37];
		unsigned char bibl_file_ident[37];
		tiso_voldesc_datetime create_time;
		tiso_voldesc_datetime modify_time;
		tiso_voldesc_datetime expr_time;
		tiso_voldesc_datetime effect_time;
		unsigned char file_struct_ver;
		unsigned char unused2;
		unsigned char app_data[512];
		unsigned char unused3[653];
	} tiso_voldesc_suppl;		// Must be 2048 bytes in size.

	typedef struct
	{
		unsigned char type;				// 3.
		unsigned char ident[5];			// "CD001".
		unsigned char version;
		unsigned char unused1;
		unsigned char sys_ident[32];
		unsigned char part_ident[32];
		unsigned char part_loc[8];		// 7.3.3.
		unsigned char part_size[8];		// 7.3.3.
		unsigned char sys_data[1960];
	} tiso_voldesc_part;	// Must be 2048 bytes in size.

	typedef struct
	{
		unsigned char type;		// 255.
		unsigned char ident[5];	// "CD001".
		unsigned char version;
		unsigned char res1[2041];
	} tiso_voldesc_setterm;		// Must be 2048 bytes in size.

#pragma pack()	// Switch back to normal alignment.

	/**
		Implements functionallity for creating parts of ISO9660 file systems.
		For example writing certain descriptors and for generating ISO9660
		compatible file names.
	*/
	class Iso9660
	{
	public:
		enum InterLevel
		{
			LEVEL_1,
			LEVEL_2,
			LEVEL_3,
			ISO9660_1999
		};

	private:
		bool relax_max_dir_level_;
		bool inc_file_ver_info_;
		InterLevel inter_level_;

		tiso_voldesc_primary voldesc_primary_;
		tiso_voldesc_setterm voldesc_setterm_;

		char MakeCharA(char c);
		char MakeCharD(char c);
        int LastDelimiterA(const char *str,char delim);
		void MemStrCopyA(unsigned char *target,const char *source,size_t len);
		void MemStrCopyD(unsigned char *target,const char *source,size_t len);

		unsigned char WriteFileNameL1(unsigned char *buffer,const ckcore::tchar *file_name);
		unsigned char WriteFileNameGeneric(unsigned char *buffer,const ckcore::tchar *file_name,int max_len);
		unsigned char WriteFileNameL2(unsigned char *buffer,const ckcore::tchar *file_name);
		unsigned char WriteFileName1999(unsigned char *buffer,const ckcore::tchar *file_name);
		unsigned char WriteDirNameL1(unsigned char *buffer,const ckcore::tchar *dir_name);
		unsigned char WriteDirNameGeneric(unsigned char *buffer,const ckcore::tchar *dir_name,int max_len);
		unsigned char WriteDirNameL2(unsigned char *buffer,const ckcore::tchar *dir_name);
		unsigned char WriteDirName1999(unsigned char *buffer,const ckcore::tchar *dir_name);
		unsigned char CalcFileNameLenL1(const ckcore::tchar *file_name);
		unsigned char CalcFileNameLenL2(const ckcore::tchar *file_name);
		unsigned char CalcFileNameLen1999(const ckcore::tchar *file_name);
		unsigned char CalcDirNameLenL1(const ckcore::tchar *file_name);
		unsigned char CalcDirNameLenL2(const ckcore::tchar *file_name);
		unsigned char CalcDirNameLen1999(const ckcore::tchar *file_name);

		void InitVolDescPrimary();
		void InitVolDescSetTerm();

	public:
		Iso9660();
		~Iso9660();

		// Change of internal state functions.
		void SetVolumeLabel(const ckcore::tchar *label);
		void SetTextFields(const ckcore::tchar *sys_ident,const ckcore::tchar *volset_ident,
						   const ckcore::tchar *publ_ident,const ckcore::tchar *prep_ident);
		void SetFileFields(const ckcore::tchar *copy_file_ident,const ckcore::tchar *abst_file_ident,
						   const ckcore::tchar *bibl_file_ident);
		void SetInterchangeLevel(InterLevel inter_level);
		void SetRelaxMaxDirLevel(bool relax);
		void SetIncludeFileVerInfo(bool include);

		// Write functions.
		bool WriteVolDescPrimary(ckcore::OutStream &out_stream,struct tm &create_time,
								 unsigned long vol_space_size,unsigned long pathtable_size,
								 unsigned long pos_pathtable_l,unsigned long pos_pathtable_m,
								 unsigned long root_extent_loc,unsigned long data_len);
		bool WriteVolDescSuppl(ckcore::OutStream &out_stream,struct tm &create_time,
							   unsigned long vol_space_size,unsigned long pathtable_size,
							   unsigned long pos_pathtable_l,unsigned long pos_pathtable_m,
							   unsigned long root_extent_loc,unsigned long data_len);
		bool WriteVolDescSetTerm(ckcore::OutStream &out_stream);

		// Helper functions.
		unsigned char WriteFileName(unsigned char *buffer,const ckcore::tchar *file_name,
									bool is_dir);
		unsigned char CalcFileNameLen(const ckcore::tchar *file_name,bool is_dir);
		unsigned char GetMaxDirLevel();
		bool HasVolDescSuppl();
		bool AllowsFragmentation();
		bool IncludesFileVerInfo();
	};

	/*
		Helper Functions.
	*/
	void write721(unsigned char *buffer,unsigned short val);			// Least significant byte first.
	void write722(unsigned char *buffer,unsigned short val);			// Most significant byte first.
	void write723(unsigned char *buffer,unsigned short val);			// Both-byte orders.
	void write72(unsigned char *buffer,unsigned short val,bool msbf);	// 7.2.1 or 7.2.2 is decided by parameter.
	void write731(unsigned char *buffer,unsigned long val);				// Least significant byte first.
	void write732(unsigned char *buffer,unsigned long val);				// Most significant byte first.
	void write733(unsigned char *buffer,unsigned long val);				// Both-byte orders.
	void write73(unsigned char *buffer,unsigned long val,bool msbf);	// 7.3.1 or 7.3.2 is decided by parameter.
	unsigned short read721(unsigned char *buffer);
	unsigned short read722(unsigned char *buffer);
	unsigned short read723(unsigned char *buffer);
	unsigned long read731(unsigned char *buffer);
	unsigned long read732(unsigned char *buffer);
	unsigned long read733(unsigned char *buffer);

	unsigned long bytes_to_sec(unsigned long bytes);
	unsigned long bytes_to_sec(ckcore::tuint64 bytes);
	ckcore::tuint64 bytes_to_sec64(ckcore::tuint64 bytes);

	void iso_make_datetime(struct tm &time,tiso_voldesc_datetime &iso_time);
	void iso_make_datetime(struct tm &time,tiso_dir_record_datetime &iso_time);
	void iso_make_datetime(unsigned short date,unsigned short time,
						   tiso_dir_record_datetime &iso_time);
	void iso_make_dosdatetime(tiso_dir_record_datetime &iso_time,
							  unsigned short &date,unsigned short &time);
};
