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
		ckcore::tuint64 file_size_;
		ckcore::tstring internal_path_;		// Path in disc image.
		ckcore::tstring external_path_;		// Path on hard drive.

		void *data_ptr_;					// Pointer to a user-defined structure, designed for CIso9660TreeNode

		FileDescriptor(const ckcore::tchar *internal_path,const ckcore::tchar *external_path,
					   ckcore::tuint64 file_size,unsigned char flags = 0,void *data_ptr = NULL) :
			internal_path_(internal_path),external_path_(external_path),
			file_size_(file_size),flags_(flags),data_ptr_(data_ptr)
		{
		}
	};

	/*
		Sorts the set of files according to the ECMA-119 standard.
	*/
	class FileComparator
	{
	private:
		bool dvd_video_;

		/*
			Returns a weight of the specified file name, a ligher file should
			be placed heigher in the directory hierarchy.
		*/
		unsigned long GetFileWeight(const ckcore::tchar *file_path) const
		{
			unsigned long weight = 0xFFFFFFFF;

			// Quick test for optimization.
			if (file_path[1] == 'V')
			{
				if (!ckcore::string::astrcmp(file_path,ckT("/VIDEO_TS")))	// The VIDEO_TS folder should be first.
					weight = 0;
				else if (!ckcore::string::astrncmp(file_path,ckT("/VIDEO_TS/"),10))
				{
					const ckcore::tchar *file_name = file_path + 10;

					if (!ckcore::string::astrncmp(file_name,ckT("VIDEO_TS"),8))
					{
						weight -= 0x80000000;

						const ckcore::tchar *file_ext = file_name + 9;
						if (!ckcore::string::astrcmp(file_ext,ckT("IFO")))
							weight -= 3;
						else if (!ckcore::string::astrcmp(file_ext,ckT("VOB")))
							weight -= 2;
						else if (!ckcore::string::astrcmp(file_ext,ckT("BUP")))
							weight -= 1;
					}
					else if (!ckcore::string::astrncmp(file_name,ckT("VTS_"),4))
					{
						weight -= 0x40000000;

						// Just a safety measure.
						if (ckcore::string::astrlen(file_name) < 64)
						{
							ckcore::tchar file_ext[64];
							unsigned long num = 0,sub_num = 0;

							if (asscanf(file_name,ckT("VTS_%u_%u.%[^\0]"),&num,&sub_num,file_ext) == 3)
							{
								// The first number is worth the most, the lower the lighter.
								weight -= 0xFFFFFF - (num << 8);

								if (!ckcore::string::astrcmp(file_ext,ckT("IFO")))
								{
									weight -= 0xFF;
								}
								else if (!ckcore::string::astrcmp(file_ext,ckT("VOB")))
								{
									weight -= 0x0F - sub_num;
								}
								else if (!ckcore::string::astrcmp(file_ext,ckT("BUP")))
								{
									weight -= 1;
								}
							}
						}
					}
				}
			}

			return weight;
		}

	public:
		/**
			@param dvd_video set to true to use DVD-Video compatible sorting.
		 */
		FileComparator(bool dvd_video) : dvd_video_(dvd_video)
		{
		}

		static int Level(const FileDescriptor &item)
		{
			const ckcore::tchar *file_path = item.internal_path_.c_str();

			int level = 0;
			for (size_t i = 0; i < (size_t)ckcore::string::astrlen(file_path); i++)
			{
				if (file_path[i] == '/' || file_path[i] == '\\')
					level++;
			}

			if (item.flags_ & FileDescriptor::FLAG_DIRECTORY)
				level++;

			return level;
		}

		bool operator() (const FileDescriptor &item1,const FileDescriptor &item2) const
		{
			if (dvd_video_)
			{
				unsigned long weight1 = GetFileWeight(item1.internal_path_.c_str());
				unsigned long weight2 = GetFileWeight(item2.internal_path_.c_str());

				if (weight1 != weight2)
				{
					if (weight1 < weight2)
						return true;
					else
						return false;
				}
			}

			int level_item1 = Level(item1);
			int level_item2 = Level(item2);

			if (level_item1 < level_item2)
				return true;
			else if (level_item1 == level_item2)
				return ckcore::string::astrcmp(item1.internal_path_.c_str(),item2.internal_path_.c_str()) < 0;
			else
				return false;
		}
	};

	typedef std::set<ckfilesystem::FileDescriptor,ckfilesystem::FileComparator> FileSet;
};
