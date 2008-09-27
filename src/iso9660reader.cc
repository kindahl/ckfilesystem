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

#include "ckfilesystem/iso9660writer.hh"
#include "ckfilesystem/iso9660reader.hh"
#include "ckfilesystem/iso9660.hh"

namespace ckfilesystem
{
	Iso9660Reader::Iso9660Reader(ckcore::Log &log) : log_(log),
		m_pRootNode(NULL)
	{
	}

	Iso9660Reader::~Iso9660Reader()
	{
		if (m_pRootNode != NULL)
		{
			delete m_pRootNode;
			m_pRootNode = NULL;
		}
	}

	/**
		Reads an entire directory entry.
	*/
	bool Iso9660Reader::ReadDirEntry(ckcore::InStream &InStream,
		std::vector<Iso9660TreeNode *> &DirEntries,
		Iso9660TreeNode *pParentNode,bool bJoliet)
	{
		// Search to the extent location.
		InStream.Seek(pParentNode->extent_loc_ * ISO9660_SECTOR_SIZE,ckcore::InStream::ckSTREAM_BEGIN);

		unsigned char ucFileNameBuffer[ISO9660WRITER_FILENAME_BUFFER_SIZE];
		ckcore::tchar szFileName[(ISO9660WRITER_FILENAME_BUFFER_SIZE / sizeof(ckcore::tchar)) + 1];

		// Skip the '.' and '..' entries.
		unsigned long ulRead = 0;
		ckcore::tint64 iProcessed = 0;

		for (unsigned int i = 0; i < 2; i++)
		{
			unsigned char ulSysDirRecLen = 0;
			iProcessed = InStream.Read(&ulSysDirRecLen,1);
			if (iProcessed == -1)
			{
				log_.PrintLine(ckT("Error: Unable to read system directory record length."));
				return false;
			}
			if (iProcessed != 1)
			{
				log_.PrintLine(ckT("Error: Unable to read system directory record length (size mismatch)."));
				return false;
			}

			InStream.Seek(ulSysDirRecLen - 1,ckcore::InStream::ckSTREAM_CURRENT);
			ulRead += ulSysDirRecLen;
		}

		tiso_dir_record CurDirRecord;

		// Read all other records.
		while (ulRead < pParentNode->extent_len_)
		{
			// Check if we can read further.
			if (ulRead + sizeof(tiso_dir_record) > pParentNode->extent_len_)
				break;

			unsigned long ulDirRecProcessed = 0;

			memset(&CurDirRecord,0,sizeof(tiso_dir_record));
			iProcessed = InStream.Read(&CurDirRecord,sizeof(tiso_dir_record) - 1);
			if (iProcessed == -1)
			{
				log_.PrintLine(ckT("Error: Unable to read directory record."));
				return false;
			}
			if (iProcessed != sizeof(tiso_dir_record) - 1)
			{
				log_.PrintLine(ckT("Error: Unable to read directory record (size mismatch: %u vs. %u)."),
					(unsigned long)iProcessed,sizeof(tiso_dir_record) - 1);
				return false;
			}
			ulDirRecProcessed += (unsigned long)iProcessed;

			if (CurDirRecord.file_ident_len > ISO9660WRITER_FILENAME_BUFFER_SIZE)
			{
				log_.PrintLine(ckT("Error: Directory record file identifier is too large: %u bytes."),
					CurDirRecord.file_ident_len);
				return false;
			}

			// Read identifier.
			iProcessed = InStream.Read(ucFileNameBuffer,CurDirRecord.file_ident_len);
			if (iProcessed == -1)
			{
				log_.PrintLine(ckT("Error: Unable to read directory record file identifier."));
				return false;
			}
			if (iProcessed != CurDirRecord.file_ident_len)
			{
				log_.PrintLine(ckT("Error: Unable to read directory record file identifier (size mismatch)."));
				return false;
			}
			ulDirRecProcessed += (unsigned long)iProcessed;

			// Convert identifier.
			if (bJoliet)
			{
#ifdef _UNICODE
				wchar_t *szWideFileName = szFileName;
#else
				wchar_t szWideFileName[(ISO9660WRITER_FILENAME_BUFFER_SIZE >> 1) + 1];
#endif
				unsigned char ucFileNameLen = CurDirRecord.file_ident_len >> 1;
				unsigned char ucFileNamePos = 0;

				for (unsigned int i = 0; i < ucFileNameLen; i++)
				{
					szWideFileName[i]  = ucFileNameBuffer[ucFileNamePos++] << 8;
					szWideFileName[i] |= ucFileNameBuffer[ucFileNamePos++];
				}

				szWideFileName[ucFileNameLen] = '\0';

#ifndef _UNICODE
                ckcore::string::utf16_to_ansi(szWideFileName,szFileName,sizeof(szFileName));
#endif
			}
			else
			{
#ifdef _UNICODE
				ckcore::string::ansi_to_utf16((const char *)ucFileNameBuffer,szFileName,sizeof(szFileName) / sizeof(ckcore::tchar));
#else
				memcpy(szFileName,ucFileNameBuffer,CurDirRecord.file_ident_len);
#endif
				szFileName[CurDirRecord.file_ident_len] = '\0';
			}

			// Remove any version information.
			unsigned char ucLength = bJoliet ? CurDirRecord.file_ident_len >> 1 :
				CurDirRecord.file_ident_len;
			if (szFileName[ucLength - 2] == ';')
				szFileName[ucLength - 2] = '\0';

			//log_.PrintLine(ckT("  %s: %u"),szFileName,read733(CurDirRecord.extent_loc));

			Iso9660TreeNode *pNewNode = new Iso9660TreeNode(pParentNode,szFileName,
					read733(CurDirRecord.extent_loc),read733(CurDirRecord.data_len),
					read723(CurDirRecord.volseq_num),CurDirRecord.file_flags,
					CurDirRecord.file_unit_size,CurDirRecord.interleave_gap_size,CurDirRecord.rec_timestamp);

			if (CurDirRecord.file_flags & DIRRECORD_FILEFLAG_DIRECTORY)
			{
				DirEntries.push_back(pNewNode);
			}

			pParentNode->m_Children.push_back(pNewNode);

			// Skip any extra data.
			if (CurDirRecord.dir_record_len - ulDirRecProcessed > 0)
			{
				InStream.Seek(CurDirRecord.dir_record_len - ulDirRecProcessed,ckcore::InStream::ckSTREAM_CURRENT);
				ulDirRecProcessed = CurDirRecord.dir_record_len;
			}

			ulRead += ulDirRecProcessed;

			// Skip any extended attribute record.
			InStream.Seek(CurDirRecord.ext_attr_record_len,ckcore::InStream::ckSTREAM_CURRENT);
			ulRead += CurDirRecord.ext_attr_record_len;

			// Ignore any zeroes.
			unsigned char ucNext;
			iProcessed = InStream.Read(&ucNext,1);
			if (iProcessed == -1)
			{
				log_.PrintLine(ckT("Error: Unable to read through zeroes."));
				return false;
			}
			if (iProcessed != 1)
			{
				log_.PrintLine(ckT("Error: Unable to read through zeroes (size mismatch)."));
				return false;
			}

			while (ucNext == 0)
			{
				ulRead++;

				iProcessed = InStream.Read(&ucNext,1);
				if (iProcessed == -1)
				{
					log_.PrintLine(ckT("Error: Unable to read through zeroes."));
					return false;
				}
				if (iProcessed != 1)
				{
					log_.PrintLine(ckT("Error: Unable to read through zeroes (size mismatch)."));
					return false;
				}
			}

			InStream.Seek(pParentNode->extent_loc_ * ISO9660_SECTOR_SIZE + ulRead,ckcore::InStream::ckSTREAM_BEGIN);
		}

		return true;
	}

	bool Iso9660Reader::Read(ckcore::InStream &InStream,unsigned long ulStartSector)
	{
		log_.PrintLine(ckT("Iso9660Reader::Read"));

		// Seek to the start sector.
		//InStream.Seek(ulStartSector * ISO9660_SECTOR_SIZE,ckcore::InStream::ckSTREAM_BEGIN);

		// Seek to sector 16.
		InStream.Seek(ISO9660_SECTOR_SIZE/* << 4*/ * (16 + ulStartSector),ckcore::InStream::ckSTREAM_BEGIN);
		if (InStream.End())
		{
			log_.PrintLine(ckT("  Error: Invalid ISO9660 file system."));
			return false;
		}

		ckcore::tint64 iProcessed = 0;

		// Read the primary volume descriptor.
		tiso_voldesc_primary VolDescPrim;
		memset(&VolDescPrim,0,sizeof(tiso_voldesc_primary));
		iProcessed = InStream.Read(&VolDescPrim,sizeof(tiso_voldesc_primary));
		if (iProcessed == -1)
		{
			log_.PrintLine(ckT("  Error: Unable to read ISO9660 file system."));
			return false;
		}
		if (iProcessed != sizeof(tiso_voldesc_primary))
		{
			log_.PrintLine(ckT("  Error: Unable to read ISO9660 primary volume descriptor."));
			return false;
		}

		// Validate primary volume descriptor.
		if (VolDescPrim.type != VOLDESCTYPE_PRIM_VOL_DESC)
		{
			log_.PrintLine(ckT("  Error: Primary volume decsriptor not found at sector 16."));
			return false;
		}

		if (VolDescPrim.version != 1)
		{
			log_.PrintLine(ckT("  Error: Bad primary volume descriptor version."));
			return false;
		}

		if (VolDescPrim.file_struct_ver != 1)
		{
			log_.PrintLine(ckT("  Error: Bad primary volume descriptor structure version."));
			return false;
		}

		if (memcmp(VolDescPrim.ident,iso_ident_cd,sizeof(VolDescPrim.ident)))
		{
			log_.PrintLine(ckT("  Error: Bad primary volume descriptor identifer."));
			return false;
		}

		// Search all other volume descriptors for a Joliet descriptor (we only search 99 maximum).
		tiso_voldesc_suppl VolDescSuppl;
		bool bJoliet = false;
		for (unsigned int i = 0; i < 99; i++)
		{
			memset(&VolDescSuppl,0,sizeof(tiso_voldesc_suppl));
			iProcessed = InStream.Read(&VolDescSuppl,sizeof(tiso_voldesc_suppl));
			if (iProcessed == -1)
			{
				log_.PrintLine(ckT("  Error: Unable to read ISO9660 file system."));
				return false;
			}
			if (iProcessed != sizeof(tiso_voldesc_suppl))
			{
				log_.PrintLine(ckT("  Error: Unable to read additional ISO9660 volume descriptor."));
				return false;
			}

			// Check if we found the terminator.
			if (VolDescSuppl.type == VOLDESCTYPE_VOL_DESC_SET_TERM)
				break;

			// Check if we found a supplementary volume descriptor.
			if (VolDescSuppl.type == VOLDESCTYPE_SUPPL_VOL_DESC)
			{
				// Check if Joliet.
				if (VolDescSuppl.esc_sec[0] == 0x25 &&
					VolDescSuppl.esc_sec[1] == 0x2F)
				{
					if (VolDescSuppl.esc_sec[2] == 0x45 ||
						VolDescSuppl.esc_sec[2] == 0x43 ||
						VolDescSuppl.esc_sec[2] == 0x40)
					{
						log_.PrintLine(ckT("  Found Joliet file system extension."));
						bJoliet = true;
						break;
					}
				}
			}
		}

		// Obtain positions of interest.
		unsigned long ulRootExtentLoc;
		unsigned long ulRootExtentLen;

		if (bJoliet)
		{
			ulRootExtentLoc = read733(VolDescSuppl.root_dir_record.extent_loc);
			ulRootExtentLen = read733(VolDescSuppl.root_dir_record.data_len);
		}
		else
		{
			ulRootExtentLoc = read733(VolDescPrim.root_dir_record.extent_loc);
			ulRootExtentLen = read733(VolDescPrim.root_dir_record.data_len);
		}

		log_.PrintLine(ckT("  Location of root directory extent: %u."),ulRootExtentLoc);
		log_.PrintLine(ckT("  Length of root directory extent: %u."),ulRootExtentLen);

		if (m_pRootNode != NULL)
			delete m_pRootNode;

		m_pRootNode = new Iso9660TreeNode(NULL,NULL,ulRootExtentLoc,ulRootExtentLen,
			read723(VolDescSuppl.root_dir_record.volseq_num),VolDescSuppl.root_dir_record.file_flags,
			VolDescSuppl.root_dir_record.file_unit_size,VolDescSuppl.root_dir_record.interleave_gap_size,
			VolDescSuppl.root_dir_record.rec_timestamp);

		std::vector<Iso9660TreeNode *> DirEntries;
		if (!ReadDirEntry(InStream,DirEntries,m_pRootNode,bJoliet))
		{
			log_.PrintLine(ckT("  Error: Failed to read directory entry at sector: %u."),ulRootExtentLoc);
			return false;
		}

		while (DirEntries.size() > 0)
		{
			Iso9660TreeNode *pParentNode = DirEntries.back();
			DirEntries.pop_back();

			if (!ReadDirEntry(InStream,DirEntries,pParentNode,bJoliet))
			{
				log_.PrintLine(ckT("  Error: Failed to read directory entry at sector: %u."),pParentNode->extent_loc_);
				return false;
			}
		}

		return true;
	}

	#ifdef _DEBUG
	void Iso9660Reader::PrintLocalTree(std::vector<std::pair<Iso9660TreeNode *,int> > &DirNodeStack,
		Iso9660TreeNode *pLocalNode,int iIndent)
	{
		std::vector<Iso9660TreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			if ((*itFile)->file_flags_ & DIRRECORD_FILEFLAG_DIRECTORY)
			{
				DirNodeStack.push_back(std::make_pair(*itFile,iIndent));
			}
			else
			{
				for (int i = 0; i < iIndent; i++)
					log_.Print(ckT(" "));

				log_.Print(ckT("<f>"));
				log_.Print((*itFile)->m_FileName.c_str());
				log_.PrintLine(ckT(" (%u:%u)"),(*itFile)->extent_loc_,(*itFile)->extent_len_);
			}
		}
	}

	void Iso9660Reader::PrintTree()
	{
		if (m_pRootNode == NULL)
			return;

		Iso9660TreeNode *pCurNode = m_pRootNode;
		int iIndent = 0;

		log_.PrintLine(ckT("Iso9660Reader::PrintTree"));
#ifdef _WINDOWS
		log_.PrintLine(ckT("  <root> (%I64u:%I64u)"),pCurNode->extent_loc_,pCurNode->extent_len_);
#else
		log_.PrintLine(ckT("  <root> (%llu:%llu)"),pCurNode->extent_loc_,pCurNode->extent_len_);
#endif

		std::vector<std::pair<Iso9660TreeNode *,int> > DirNodeStack;
		PrintLocalTree(DirNodeStack,pCurNode,4);

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1].first;
			iIndent = DirNodeStack[DirNodeStack.size() - 1].second;

			DirNodeStack.pop_back();

			// Print the directory name.
			for (int i = 0; i < iIndent; i++)
				log_.Print(ckT(" "));

			log_.Print(ckT("<d>"));
			log_.Print(pCurNode->m_FileName.c_str());
#ifdef _WINDOWS
			log_.PrintLine(ckT(" (%I64u:%I64u)"),pCurNode->extent_loc_,pCurNode->extent_len_);
#else
			log_.PrintLine(ckT(" (%llu:%llu)"),pCurNode->extent_loc_,pCurNode->extent_len_);
#endif

			PrintLocalTree(DirNodeStack,pCurNode,iIndent + 2);
		}
	}
#endif
};
