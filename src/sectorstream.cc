/*
 * Copyright (C) 2006-2008 Christian Kindahl, christian dot kindahl at gmail dot com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "stdafx.h"
#include "SectorStream.h"

namespace ckFileSystem
{
	/*
		COutBufferedStream
	*/
	CSectorOutStream::CSectorOutStream(ckcore::OutStream &OutStream,
									   unsigned long ulSectorSize) :
		ckcore::BufferedOutStream(OutStream)
	{
		m_ulSectorSize = ulSectorSize;
		m_uiSector = 0;		// We start at sector 0.
		m_uiWritten = 0;
	}

	CSectorOutStream::~CSectorOutStream()
	{
	}

	ckcore::tint64 CSectorOutStream::Write(void *pBuffer,ckcore::tuint32 uiCount)
	{
		ckcore::tint64 iResult = ckcore::BufferedOutStream::Write(pBuffer,uiCount);
		m_uiWritten += iResult;

		while (m_uiWritten >= m_ulSectorSize)
		{
			m_uiWritten -= m_ulSectorSize;
			m_uiSector++;
		}

		return iResult;
	}

	/*
		Returns the current sector number.
	*/
	unsigned __int64 CSectorOutStream::GetSector()
	{
		return m_uiSector;
	}

	/*
		Returns the number of buytes that's allocated in the current sector.
	*/
	unsigned long CSectorOutStream::GetAllocated()
	{
		return (unsigned long)m_uiWritten;
	}

	/*
		Returns the remaining unallocated bytes in the current sector.
	*/
	unsigned long CSectorOutStream::GetRemaining()
	{
		return m_ulSectorSize - (unsigned long)m_uiWritten;
	}

	/*
		Pads the remaining bytes of the current sector with 0s.
	*/
	void CSectorOutStream::PadSector()
	{
		char szTemp[1] = { 0 };

		unsigned long ulRemaining = GetRemaining();

		for (unsigned long i = 0; i < ulRemaining; i++)
			Write(szTemp,1);
	}
};
