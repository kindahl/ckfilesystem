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

#include <time.h>
#include <ckcore/file.hh>
#include <ckcore/directory.hh>
#include "ckfilesystem/udfwriter.hh"
#include "ckfilesystem/iso9660.hh"

namespace ckfilesystem
{
	UdfWriter::UdfWriter(ckcore::Log &log,SectorOutStream &out_stream,
		SectorManager &sec_manager,Udf &udf,bool bUseFileTimes) :
		log_(log),out_stream_(out_stream),sec_manager_(sec_manager),
		udf_(udf),m_bUseFileTimes(bUseFileTimes),m_uiPartLength(0)
	{
		memset(&m_MainVolDescSeqExtent,0,sizeof(tUdfExtentAd));
		memset(&m_ReserveVolDescSeqExtent,0,sizeof(tUdfExtentAd));

		// Get local system time.
		time_t CurrentTime;
		time(&CurrentTime);

		m_ImageCreate = *localtime(&CurrentTime);
	}

	UdfWriter::~UdfWriter()
	{
	}

	void UdfWriter::CalcLocalNodeLengths(std::vector<FileTreeNode *> &DirNodeStack,
		FileTreeNode *pLocalNode)
	{
		pLocalNode->m_uiUdfSize = 0;
		pLocalNode->m_uiUdfSize += BytesToSector(udf_.CalcFileEntrySize());
		pLocalNode->m_uiUdfSize += BytesToSector(CalcIdentSize(pLocalNode));
		pLocalNode->m_uiUdfSizeTot = pLocalNode->m_uiUdfSize;

		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
				DirNodeStack.push_back(*itFile);
			else
			{
				(*itFile)->m_uiUdfSize = BytesToSector(udf_.CalcFileEntrySize());
				(*itFile)->m_uiUdfSizeTot = (*itFile)->m_uiUdfSize;
			}
		}
	}

	void UdfWriter::CalcNodeLengths(FileTree &file_tree)
	{
		FileTreeNode *pCurNode = file_tree.GetRoot();

		std::vector<FileTreeNode *> DirNodeStack;
		CalcLocalNodeLengths(DirNodeStack,pCurNode);

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack[DirNodeStack.size() - 1];
			DirNodeStack.pop_back();

			CalcLocalNodeLengths(DirNodeStack,pCurNode);
		}
	}

	ckcore::tuint64 UdfWriter::CalcIdentSize(FileTreeNode *pLocalNode)
	{
		ckcore::tuint64 uiTotalIdentSize = 0;
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			uiTotalIdentSize += udf_.CalcFileIdentSize((*itFile)->m_FileName.c_str());
		}

		// Don't forget to add the '..' item to the total.
		uiTotalIdentSize += udf_.CalcFileIdentParentSize();

		return uiTotalIdentSize;
	}

	/*
		FIXME: This function uses recursion, it should be converted to an
		iterative function to avoid the risk of running out of memory.
	*/
	ckcore::tuint64 UdfWriter::CalcNodeSizeTotal(FileTreeNode *pLocalNode)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			pLocalNode->m_uiUdfSizeTot += CalcNodeSizeTotal(*itFile);
		}

		return pLocalNode->m_uiUdfSizeTot;
	}

	/*
		FIXME: This function uses recursion, it should be converted to an
		iterative function to avoid the risk of running out of memory.
	*/
	ckcore::tuint64 UdfWriter::CalcNodeLinksTotal(FileTreeNode *pLocalNode)
	{
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			pLocalNode->m_uiUdfLinkTot += CalcNodeLinksTotal(*itFile);
		}

		return (pLocalNode->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY) ? 1 : 0;
	}

	/**
		Calculates the number of bytes needed to store the UDF partition.
	*/
	ckcore::tuint64 UdfWriter::CalcParitionLength(FileTree &file_tree)
	{
		// First wee need to calculate the individual sizes of each records.
		CalcNodeLengths(file_tree);

		// Calculate the number of directory links associated with each directory node.
		CalcNodeLinksTotal(file_tree.GetRoot());

		// The update the compelte tree (unfortunately recursively).
		return CalcNodeSizeTotal(file_tree.GetRoot());
	}

	bool UdfWriter::WriteLocalParitionDir(std::deque<FileTreeNode *> &DirNodeQueue,
		FileTreeNode *pLocalNode,unsigned long &ulCurPartSec,ckcore::tuint64 &uiUniqueIdent)
	{
		unsigned long ulEntrySec = ulCurPartSec++;
		unsigned long ulIdentSec = ulCurPartSec;	// On folders the identifiers will follow immediately.

		// Calculate the size of all identifiers.
		ckcore::tuint64 uiTotalIdentSize = 0;
		std::vector<FileTreeNode *>::const_iterator itFile;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			uiTotalIdentSize += udf_.CalcFileIdentSize((*itFile)->m_FileName.c_str());
		}

		// Don't forget to add the '..' item to the total.
		uiTotalIdentSize += udf_.CalcFileIdentParentSize();

		unsigned long ulNextEntrySec = ulIdentSec + BytesToSector(uiTotalIdentSize);

		// Get file modified dates.
		struct tm AccessTime,ModifyTime,CreateTime;
		if (!ckcore::Directory::Time(pLocalNode->m_FileFullPath.c_str(),AccessTime,ModifyTime,CreateTime))
			AccessTime = ModifyTime = CreateTime = m_ImageCreate;

		// The current folder entry.
		if (!udf_.WriteFileEntry(out_stream_,ulEntrySec,true,(unsigned short)pLocalNode->m_uiUdfLinkTot + 1,
			uiUniqueIdent,ulIdentSec,uiTotalIdentSize,AccessTime,ModifyTime,CreateTime))
		{
			return false;
		}

		// Unique identifiers 0-15 are reserved for Macintosh implementations.
		if (uiUniqueIdent == 0)
			uiUniqueIdent = 16;
		else
			uiUniqueIdent++;

		// The '..' item.
		unsigned long ulParentEntrySec = pLocalNode->GetParent() == NULL ? ulEntrySec : pLocalNode->GetParent()->m_ulUdfPartLoc;
		if (!udf_.WriteFileIdentParent(out_stream_,ulCurPartSec,ulParentEntrySec))
			return false;

		// Keep track on how many bytes we have in our sector.
		unsigned long ulSectorBytes = udf_.CalcFileIdentParentSize();

		std::vector<FileTreeNode *> TempStack;
		for (itFile = pLocalNode->m_Children.begin(); itFile !=
			pLocalNode->m_Children.end(); itFile++)
		{
			// Push the item to the temporary stack.
			TempStack.push_back(*itFile);

			if ((*itFile)->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
			{
				if (!udf_.WriteFileIdent(out_stream_,ulCurPartSec,ulNextEntrySec,true,(*itFile)->m_FileName.c_str()))
					return false;

				(*itFile)->m_ulUdfPartLoc = ulNextEntrySec;	// Remember where this entry was stored.

				ulNextEntrySec += (unsigned long)(*itFile)->m_uiUdfSizeTot;
			}
			else
			{
				if (!udf_.WriteFileIdent(out_stream_,ulCurPartSec,ulNextEntrySec,false,(*itFile)->m_FileName.c_str()))
					return false;

				(*itFile)->m_ulUdfPartLoc = ulNextEntrySec;	// Remember where this entry was stored.

				ulNextEntrySec += (unsigned long)(*itFile)->m_uiUdfSizeTot;
			}

			ulSectorBytes += udf_.CalcFileIdentSize((*itFile)->m_FileName.c_str());
			if (ulSectorBytes >= UDF_SECTOR_SIZE)
			{
				ulCurPartSec++;
				ulSectorBytes -= UDF_SECTOR_SIZE;
			}
		}

		// Insert the elements from the temporary stack into the queue.
		std::vector<FileTreeNode *>::reverse_iterator itStackNode;
		for (itStackNode = TempStack.rbegin(); itStackNode != TempStack.rend(); itStackNode++)
			DirNodeQueue.push_front(*itStackNode);

		// Pad to the next sector.
		out_stream_.PadSector();

		if (ulSectorBytes > 0)
			ulCurPartSec++;

		return true;
	}

	bool UdfWriter::WritePartitionEntries(FileTree &file_tree)
	{
		// We start at partition sector 1, sector 0 is the parition anchor descriptor.
		unsigned long ulCurPartSec = 1;

		// We start with unique identifier 0 (which is reserved for root) and
		// increase it for every file or folder added.
		ckcore::tuint64 uiUniqueIdent = 0;

		FileTreeNode *pCurNode = file_tree.GetRoot();
		pCurNode->m_ulUdfPartLoc = ulCurPartSec;

		std::deque<FileTreeNode *> DirNodeStack;
		if (!WriteLocalParitionDir(DirNodeStack,pCurNode,ulCurPartSec,uiUniqueIdent))
			return false;

		unsigned long ulTemp = 0;

		while (DirNodeStack.size() > 0)
		{ 
			pCurNode = DirNodeStack.front();
			DirNodeStack.pop_front();

#ifdef _DEBUG
			if (pCurNode->m_ulUdfPartLoc != ulCurPartSec)
			{
				log_.PrintLine(ckT("Invalid location for \"%s\" in UDF file system. Proposed position %u verus actual position %u."),
					pCurNode->m_FileFullPath.c_str(),pCurNode->m_ulUdfPartLoc,ulCurPartSec);
			}
#endif

			if (pCurNode->m_ucFileFlags & FileTreeNode::FLAG_DIRECTORY)
			{
				if (!WriteLocalParitionDir(DirNodeStack,pCurNode,ulCurPartSec,uiUniqueIdent))
					return false;
			}
			else
			{
				ulTemp++;

				// Get file modified dates.
				struct tm AccessTime,ModifyTime,CreateTime;
				if (m_bUseFileTimes && !ckcore::File::Time(pCurNode->m_FileFullPath.c_str(),AccessTime,ModifyTime,CreateTime))
					AccessTime = ModifyTime = CreateTime = m_ImageCreate;

				if (!udf_.WriteFileEntry(out_stream_,ulCurPartSec++,false,1,
					uiUniqueIdent,(unsigned long)pCurNode->m_uiDataPosNormal - 257,pCurNode->m_uiFileSize,
					AccessTime,ModifyTime,CreateTime))
				{
					return false;
				}

				// Unique identifiers 0-15 are reserved for Macintosh implementations.
				if (uiUniqueIdent == 0)
					uiUniqueIdent = UDF_UNIQUEIDENT_MIN;
				else
					uiUniqueIdent++;
			}
		}

		return true;
	}

	int UdfWriter::AllocateHeader()
	{
		sec_manager_.AllocateBytes(this,SR_INITIALDESCRIPTORS,udf_.GetVolDescInitialSize());
		return RESULT_OK;
	}

	int UdfWriter::AllocatePartition(FileTree &file_tree)
	{
		// Allocate everything up to sector 258.
		sec_manager_.AllocateSectors(this,SR_MAINDESCRIPTORS,258 - sec_manager_.GetNextFree());

		// Allocate memory for the file set contents.
		m_uiPartLength = CalcParitionLength(file_tree);

		sec_manager_.AllocateSectors(this,SR_FILESETCONTENTS,m_uiPartLength);

		return RESULT_OK;
	}

	int UdfWriter::WriteHeader()
	{
		if (!udf_.WriteVolDescInitial(out_stream_))
		{
			log_.PrintLine(ckT("  Error: Failed to write initial UDF volume descriptors."));
			return RESULT_FAIL;
		}

		return RESULT_OK;
	}

	int UdfWriter::WritePartition(FileTree &file_tree)
	{
		if (m_uiPartLength == 0)
		{
			log_.PrintLine(ckT("  Error: Cannot write UDF parition since no space has been reserved for it."));
			return RESULT_FAIL;
		}

		if (m_uiPartLength > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: UDF partition is too large (%I64u sectors)."),m_uiPartLength);
#else
			log_.PrintLine(ckT("  Error: UDF partition is too large (%llu sectors)."),m_uiPartLength);
#endif
			return RESULT_FAIL;
		}

		if (sec_manager_.GetStart(this,SR_MAINDESCRIPTORS) > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: Error during sector space allocation. Start of UDF main descriptors %I64d."),
#else
			log_.PrintLine(ckT("  Error: Error during sector space allocation. Start of UDF main descriptors %lld."),
#endif
				sec_manager_.GetStart(this,SR_MAINDESCRIPTORS));
			return RESULT_FAIL;
		}

		if (sec_manager_.GetDataLength() > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: File data too large (%I64d sectors) for UDF."),sec_manager_.GetDataLength());
#else
			log_.PrintLine(ckT("  Error: File data too large (%lld sectors) for UDF."),sec_manager_.GetDataLength());
#endif
			return RESULT_FAIL;
		}

		// Used for padding.
		char szTemp[1] = { 0 };

		// Parition size = the partition size calculated above + the set descriptor + the data length.
		unsigned long ulUdfCurSec = (unsigned long)sec_manager_.GetStart(this,SR_MAINDESCRIPTORS);
		unsigned long ulPartLength = (unsigned long)m_uiPartLength + 1 + (unsigned long)sec_manager_.GetDataLength();

		// Assign a unique identifier that's larger than any unique identifier of a
		// file entry + 16 for the reserved numbers.
		ckcore::tuint64 uiUniqueIdent = file_tree.GetDirCount() + file_tree.GetFileCount() + 1 + UDF_UNIQUEIDENT_MIN;

		tUdfExtentAd IntegritySeqExtent;
		IntegritySeqExtent.ulExtentLen = UDF_SECTOR_SIZE;
		IntegritySeqExtent.ulExtentLoc = ulUdfCurSec;

		if (!udf_.WriteVolDescLogIntegrity(out_stream_,ulUdfCurSec,file_tree.GetFileCount(),
			file_tree.GetDirCount() + 1,ulPartLength,uiUniqueIdent,m_ImageCreate))
		{
			log_.PrintLine(ckT("  Error: Failed to write UDF logical integrity descriptor."));
			return RESULT_FAIL;
		}
		ulUdfCurSec++;			

		// 6 volume descriptors. But minimum length is 16 so we pad with empty sectors.
		m_MainVolDescSeqExtent.ulExtentLen = m_ReserveVolDescSeqExtent.ulExtentLen = 16 * ISO9660_SECTOR_SIZE;

		// We should write the set of volume descriptors twice.
		for (unsigned int i = 0; i < 2; i++)
		{
			// Remember the start of the volume descriptors.
			if (i == 0)
				m_MainVolDescSeqExtent.ulExtentLoc = ulUdfCurSec;
			else
				m_ReserveVolDescSeqExtent.ulExtentLoc = ulUdfCurSec;

			unsigned long ulVolDescSeqNum = 0;
			if (!udf_.WriteVolDescPrimary(out_stream_,ulVolDescSeqNum++,ulUdfCurSec,m_ImageCreate))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF primary volume descriptor."));
				return RESULT_FAIL;
			}
			ulUdfCurSec++;

			if (!udf_.WriteVolDescImplUse(out_stream_,ulVolDescSeqNum++,ulUdfCurSec))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF implementation use descriptor."));
				return RESULT_FAIL;
			}
			ulUdfCurSec++;

			if (!udf_.WriteVolDescPartition(out_stream_,ulVolDescSeqNum++,ulUdfCurSec,257,ulPartLength))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF partition descriptor."));
				return RESULT_FAIL;
			}
			ulUdfCurSec++;

			if (!udf_.WriteVolDescLogical(out_stream_,ulVolDescSeqNum++,ulUdfCurSec,IntegritySeqExtent))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF logical partition descriptor."));
				return RESULT_FAIL;
			}
			ulUdfCurSec++;

			if (!udf_.WriteVolDescUnalloc(out_stream_,ulVolDescSeqNum++,ulUdfCurSec))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF unallocated space descriptor."));
				return RESULT_FAIL;
			}
			ulUdfCurSec++;

			if (!udf_.WriteVolDescTerm(out_stream_,ulUdfCurSec))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF descriptor terminator."));
				return RESULT_FAIL;
			}
			ulUdfCurSec++;

			// According to UDF 1.02 standard each volume descriptor
			// sequence extent must contain atleast 16 sectors. Because of
			// this we need to add 10 empty sectors.
			for (int j = 0; j < 10; j++)
			{
				for (unsigned long i = 0; i < ISO9660_SECTOR_SIZE; i++)
					out_stream_.Write(szTemp,1);
				ulUdfCurSec++;
			}
		}

		// Allocate everything until sector 256 with empty sectors.
		while (ulUdfCurSec < 256)
		{
			for (unsigned long i = 0; i < ISO9660_SECTOR_SIZE; i++)
				out_stream_.Write(szTemp,1);
			ulUdfCurSec++;
		}

		// At sector 256 write the first anchor volume descriptor pointer.
		if (!udf_.WriteAnchorVolDescPtr(out_stream_,ulUdfCurSec,m_MainVolDescSeqExtent,m_ReserveVolDescSeqExtent))
		{
			log_.PrintLine(ckT("  Error: Failed to write anchor volume descriptor pointer."));
			return RESULT_FAIL;
		}
		ulUdfCurSec++;

		// The file set descriptor is the first entry in the partition, hence the logical block address 0.
		// The root is located directly after this descriptor, hence the location 1.
		if (!udf_.WriteFileSetDesc(out_stream_,0,1,m_ImageCreate))
		{
			log_.PrintLine(ckT("  Error: Failed to write file set descriptor."));
			return RESULT_FAIL;
		}

		if (!WritePartitionEntries(file_tree))
		{
			log_.PrintLine(ckT("  Error: Failed to write file UDF partition."));
			return RESULT_FAIL;
		}

		return RESULT_OK;
	}

	int UdfWriter::WriteTail()
	{
		ckcore::tuint64 uiLastDataSector = sec_manager_.GetDataStart() +
			sec_manager_.GetDataLength();
		if (uiLastDataSector > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: File data too large, last data sector is %I64d."),uiLastDataSector);
#else
			log_.PrintLine(ckT("  Error: File data too large, last data sector is %lld."),uiLastDataSector);
#endif
			return RESULT_FAIL;
		}

		// Finally write the 2nd and last anchor volume descriptor pointer.
		if (!udf_.WriteAnchorVolDescPtr(out_stream_,(unsigned long)uiLastDataSector,
			m_MainVolDescSeqExtent,m_ReserveVolDescSeqExtent))
		{
			log_.PrintLine(ckT("  Error: Failed to write anchor volume descriptor pointer."));
			return RESULT_FAIL;
		}

		out_stream_.PadSector();
		return RESULT_OK;
	}
};
