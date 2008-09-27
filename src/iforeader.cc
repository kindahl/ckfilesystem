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

#include <ckcore/convert.hh>
#include "ckfilesystem/dvdvideo.hh"
#include "ckfilesystem/iforeader.hh"

namespace ckFileSystem
{
	IfoReader::IfoReader(const ckcore::tchar *szFullPath) : m_InStream(szFullPath)
	{
		m_IfoType = IT_UNKNOWN;
	}

	IfoReader::~IfoReader()
	{
		Close();
	}

	/**
		Open the IFO file and determine it's type.
		@param szFullPath the full file path to the file to open.
		@return true if the file was successfully opened and identified, false
		otherwise.
	*/
	bool IfoReader::Open()
	{
		if (!m_InStream.Open())
			return false;

		char szIdentifier[IFO_IDENTIFIER_LEN + 1];
		ckcore::tint64 iProcessed = m_InStream.Read(szIdentifier,IFO_IDENTIFIER_LEN);
		if (iProcessed == -1)
		{
			Close();
			return false;
		}

		szIdentifier[IFO_IDENTIFIER_LEN] = '\0';
		if (!strcmp(szIdentifier,IFO_INDETIFIER_VMG))
			m_IfoType = IT_VMG;
		else if (!strcmp(szIdentifier,IFO_INDETIFIER_VTS))
			m_IfoType = IT_VTS;
		else
		{
			Close();
			return false;
		}

		return true;
	}

	bool IfoReader::Close()
	{
		m_IfoType = IT_UNKNOWN;
		return m_InStream.Close();
	}

	bool IfoReader::ReadVmg(IfoVmgData &VmgData)
	{
		// Read last sector of VMG.
		unsigned long ulSector = 0;
		ckcore::tint64 iProcessed = 0;

		m_InStream.Seek(12,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VmgData.ulLastVmgSector = ckcore::convert::be_to_le32(ulSector);

		// Read last sector of IFO.
		m_InStream.Seek(28,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VmgData.ulLastVmgIfoSector = ckcore::convert::be_to_le32(ulSector);

		// Read number of VTS  title sets.
		unsigned short usNumTitles;

		m_InStream.Seek(62,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&usNumTitles,sizeof(usNumTitles));
		if (iProcessed == -1)
			return false;

		VmgData.usNumVmgTitleSets = ckcore::convert::be_to_le16(usNumTitles);

		// Read start sector of VMG Menu VOB.
		m_InStream.Seek(192,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VmgData.ulVmgMenuVobSector = ckcore::convert::be_to_le32(ulSector);

		// Read sector offset to TT_SRPT.
		m_InStream.Seek(196,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VmgData.ulSrptSector = ckcore::convert::be_to_le32(ulSector);

		// Read the TT_SRPT titles.
		m_InStream.Seek(DVDVIDEO_BLOCK_SIZE * VmgData.ulSrptSector,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&usNumTitles,sizeof(usNumTitles));
		if (iProcessed == -1)
			return false;

		usNumTitles = ckcore::convert::be_to_le16(usNumTitles);
		for (unsigned short i = 0; i < usNumTitles; i++)
		{
			m_InStream.Seek((DVDVIDEO_BLOCK_SIZE * VmgData.ulSrptSector) + 8 + (i * 12) + 8,ckcore::InStream::ckSTREAM_BEGIN);
			iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
			if (iProcessed == -1)
				return false;

			VmgData.Titles.push_back(ckcore::convert::be_to_le32(ulSector));
		}

		return true;
	}

	bool IfoReader::ReadVts(IfoVtsData &VtsData)
	{
		// Read last sector of VTS.
		unsigned long ulSector = 0;
		ckcore::tint64 iProcessed = 0;

		m_InStream.Seek(12,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VtsData.ulLastVtsSector = ckcore::convert::be_to_le32(ulSector);

		// Read last sector of IFO.
		m_InStream.Seek(28,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VtsData.ulLastVtsIfoSector = ckcore::convert::be_to_le32(ulSector);

		// Read start sector of VTS Menu VOB.
		m_InStream.Seek(192,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VtsData.ulVtsMenuVobSector = ckcore::convert::be_to_le32(ulSector);

		// Read start sector of VTS Title VOB.
		m_InStream.Seek(196,ckcore::InStream::ckSTREAM_BEGIN);
		iProcessed = m_InStream.Read(&ulSector,sizeof(ulSector));
		if (iProcessed == -1)
			return false;

		VtsData.ulVtsVobSector = ckcore::convert::be_to_le32(ulSector);
		return true;
	}

	IfoReader::eIfoType IfoReader::GetType()
	{
		return m_IfoType;
	}
};
