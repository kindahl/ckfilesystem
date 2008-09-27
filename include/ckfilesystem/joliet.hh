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

#define JOLIET_MAX_NAMELEN_NORMAL			 64		// According to Joliet specification.
#define JOLIET_MAX_NAMELEN_RELAXED			101		// 207 bytes = 101 wide characters + 4 wide characters for file version.

namespace ckfilesystem
{
	class Joliet
	{
	private:
		bool inc_file_ver_info_;
		int max_name_len_;

		tiso_voldesc_suppl voldesc_suppl_;

		wchar_t MakeChar(wchar_t c);
        int LastDelimiterW(const wchar_t *str,wchar_t delim);
		void MemStrCopy(unsigned char *target,const wchar_t *source,size_t len);
		void EmptyStrBuffer(unsigned char *buffer,size_t size);

		void InitVolDesc();

	public:
		Joliet();
		~Joliet();

		// Change of internal state functions.
		void SetVolumeLabel(const ckcore::tchar *label);
		void SetTextFields(const ckcore::tchar *sys_ident,const ckcore::tchar *volset_ident,
			const ckcore::tchar *publ_ident,const ckcore::tchar *prep_ident);
		void SetFileFields(const ckcore::tchar *copy_file_ident,const ckcore::tchar *abst_file_ident,
			const ckcore::tchar *bibl_file_ident);
		void SetIncludeFileVerInfo(bool include);
		void SetRelaxMaxNameLen(bool relax);

		// Write functions.
		bool WriteVolDesc(ckcore::OutStream &out_stream,struct tm &create_time,
			unsigned long vol_space_size,unsigned long pathtable_size,
			unsigned long pos_pathtable_l,unsigned long pos_pathtable_m,
			unsigned long root_extent_loc,unsigned long data_len);

		// Helper functions.
		unsigned char WriteFileName(unsigned char *buffer,const ckcore::tchar *file_name,bool is_dir);
		unsigned char CalcFileNameLen(const ckcore::tchar *file_name,bool is_dir);
		bool IncludesFileVerInfo();
	};
};
