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

#include "ckfilesystem/stringtable.hh"

namespace ckfilesystem
{
	StringTable::StringTable()
	{
		m_Strings[WARNING_FSDIRLEVEL] = ckT("The directory structure is deeper than %d levels. Deep files and folders will be ignored.");
		m_Strings[WARNING_SKIPFILE] = ckT("Skipping \"%s\".");
		m_Strings[WARNING_SKIP4GFILE] = ckT("Skipping \"%s\", the file is larger than 4 GiB.");
		m_Strings[WARNING_SKIP4GFILEISO] = ckT("The file \"%s\" is larger than 4 GiB. It will not be visible in the ISO9660/Joliet file system.");
		m_Strings[ERROR_PATHTABLESIZE] = ckT("The disc image path table is to large. The project contains too many files.");
		m_Strings[ERROR_OPENWRITE] = ckT("Unable to open file for writing: %s.");
		m_Strings[ERROR_OPENREAD] = ckT("Unable to open file for reading: %s.");
		m_Strings[STATUS_BUILDTREE] = ckT("Building file tree.");
		m_Strings[STATUS_WRITEDATA] = ckT("Writing file data.");
		m_Strings[STATUS_WRITEISOTABLE] = ckT("Writing ISO9660 path tables.");
		m_Strings[STATUS_WRITEJOLIETTABLE] = ckT("Writing Joliet path tables.");
		m_Strings[STATUS_WRITEDIRENTRIES] = ckT("Writing directory entries.");
	}

	StringTable::~StringTable()
	{
	}

	StringTable &StringTable::Instance()
	{
		static StringTable instance;
		return instance;
	}

	const ckcore::tchar *StringTable::GetString(StringsId StringID)
	{
		return m_Strings[StringID];
	}

	/*
		For translation purposes.
	*/
	void StringTable::SetString(StringsId StringID,const ckcore::tchar *szString)
	{
		m_Strings[StringID] = szString;
	}
};
