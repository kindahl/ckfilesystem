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

#include "ckfilesystem/sectorstream.hh"

namespace ckfilesystem
{
	/*
		COutBufferedStream
	*/
	SectorOutStream::SectorOutStream(ckcore::OutStream &out_stream,
									 ckcore::tuint32 sector_size) :
		ckcore::BufferedOutStream(out_stream),sector_size_(sector_size),
		sector_(0),written_(0)
	{
	}

	SectorOutStream::~SectorOutStream()
	{
	}

	ckcore::tint64 SectorOutStream::write(void *buffer,ckcore::tuint32 count)
	{
		ckcore::tint64 res = ckcore::BufferedOutStream::write(buffer,count);
		written_ += res;

		while (written_ >= sector_size_)
		{
			written_ -= sector_size_;
			sector_++;
		}

		return res;
	}

	/*
		Returns the current sector number.
	*/
	ckcore::tuint64 SectorOutStream::get_sector()
	{
		return sector_;
	}

	/*
		Returns the number of buytes that's allocated in the current sector.
	*/
	ckcore::tuint32 SectorOutStream::get_allocated()
	{
		return (ckcore::tuint32)written_;
	}

	/*
		Returns the remaining unallocated bytes in the current sector.
	*/
	ckcore::tuint32 SectorOutStream::get_remaining()
	{
		return sector_size_ - (ckcore::tuint32)written_;
	}

	/*
		Pads the remaining bytes of the current sector with 0s.
	*/
	void SectorOutStream::pad_sector()
	{
		char tmp[1] = { 0 };

		ckcore::tuint32 remaining = get_remaining();
		for (ckcore::tuint32 i = 0; i < remaining; i++)
			write(tmp,1);
	}
};
