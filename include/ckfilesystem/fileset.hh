/*
 * The ckFileSystem library provides file system functionality.
 * Copyright (C) 2006-2009 Christian Kindahl
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
#include <set>
#include <ckcore/types.hh>
#include <ckcore/string.hh>

namespace ckfilesystem
{
	/*
		Describes a file that should be included in the disc image.
	*/
	class FileDescriptor
	{
	public:
		enum
		{
			FLAG_DIRECTORY = 0x01,
			FLAG_IMPORTED = 0x02
		};

		unsigned char flags_;
		ckcore::tstring internal_path_;		// Path in disc image.
		ckcore::tstring external_path_;		// Path on hard drive.

		void *data_ptr_;					// Pointer to a user-defined structure, designed for CIso9660TreeNode

		FileDescriptor(const ckcore::tchar *internal_path,const ckcore::tchar *external_path,
					   unsigned char flags = 0,void *data_ptr = NULL) :
            flags_(flags),
            internal_path_(internal_path),external_path_(external_path),
			data_ptr_(data_ptr)
		{
		}
	};

	/*
		Sorts the set of files according to the ECMA-119 standard.
	*/
	class FileComparator
	{
	public:
		bool operator() (const FileDescriptor &item1,const FileDescriptor &item2) const
		{
			return ckcore::string::astrcmp(item1.internal_path_.c_str(),
										   item2.internal_path_.c_str()) < 0;
		}
	};

	typedef std::set<ckfilesystem::FileDescriptor,ckfilesystem::FileComparator> FileSet;
};
