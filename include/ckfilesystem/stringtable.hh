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
#include <map>
#include <ckcore/types.hh>

namespace ckfilesystem
{
	enum StringsId
	{
		WARNING_FSDIRLEVEL,
		WARNING_SKIPFILE,
		WARNING_SKIP4GFILE,
		WARNING_SKIP4GFILEISO,
		ERROR_PATHTABLESIZE,
		ERROR_OPENWRITE,
		ERROR_OPENREAD,
		STATUS_BUILDTREE,
		STATUS_WRITEDATA,
		STATUS_WRITEISOTABLE,
		STATUS_WRITEJOLIETTABLE,
		STATUS_WRITEDIRENTRIES
	};

	class StringTable
	{
	private:
		std::map<StringsId,const ckcore::tchar *> m_Strings;

		StringTable();
		StringTable(const StringTable &obj);
		~StringTable();
		StringTable &operator=(const StringTable &obj); 

	public:

		static StringTable &Instance();

		const ckcore::tchar *GetString(StringsId StringID);
		void SetString(StringsId StringID,const ckcore::tchar *szString);
	};
};
