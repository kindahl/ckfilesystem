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
	UdfWriter::UdfWriter(ckcore::Log &log,SectorOutStream &out_stream,SectorManager &sec_manager,
		Udf &udf,bool use_file_times) :
		log_(log),out_stream_(out_stream),sec_manager_(sec_manager),
		udf_(udf),use_file_times_(use_file_times),part_len_(0)
	{
		memset(&voldesc_seqextent_main_,0,sizeof(tudf_extent_ad));
		memset(&voldesc_seqextent_rsrv_,0,sizeof(tudf_extent_ad));

		// Get local system time.
		time_t cur_time;
		time(&cur_time);

		create_time_ = *localtime(&cur_time);
	}

	UdfWriter::~UdfWriter()
	{
	}

	void UdfWriter::CalcLocalNodeLengths(std::vector<FileTreeNode *> &dir_node_stack,
		FileTreeNode *local_node)
	{
		local_node->m_uiUdfSize = 0;
		local_node->m_uiUdfSize += bytes_to_sec(udf_.CalcFileEntrySize());
		local_node->m_uiUdfSize += bytes_to_sec(CalcIdentSize(local_node));
		local_node->m_uiUdfSizeTot = local_node->m_uiUdfSize;

		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->m_Children.begin(); it !=
			local_node->m_Children.end(); it++)
		{
			if ((*it)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
				dir_node_stack.push_back(*it);
			else
			{
				(*it)->m_uiUdfSize = bytes_to_sec(udf_.CalcFileEntrySize());
				(*it)->m_uiUdfSizeTot = (*it)->m_uiUdfSize;
			}
		}
	}

	void UdfWriter::CalcNodeLengths(FileTree &file_tree)
	{
		FileTreeNode *cur_node = file_tree.GetRoot();

		std::vector<FileTreeNode *> dir_node_stack;
		CalcLocalNodeLengths(dir_node_stack,cur_node);

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack[dir_node_stack.size() - 1];
			dir_node_stack.pop_back();

			CalcLocalNodeLengths(dir_node_stack,cur_node);
		}
	}

	ckcore::tuint64 UdfWriter::CalcIdentSize(FileTreeNode *local_node)
	{
		ckcore::tuint64 tot_ident_size = 0;
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->m_Children.begin(); it !=
			local_node->m_Children.end(); it++)
		{
			tot_ident_size += udf_.CalcFileIdentSize((*it)->m_FileName.c_str());
		}

		// Don't forget to add the '..' item to the total.
		tot_ident_size += udf_.CalcFileIdentParentSize();
		return tot_ident_size;
	}

	/*
		FIXME: This function uses recursion, it should be converted to an
		iterative function to avoid the risk of running out of memory.
	*/
	ckcore::tuint64 UdfWriter::CalcNodeSizeTotal(FileTreeNode *local_node)
	{
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->m_Children.begin(); it !=
			local_node->m_Children.end(); it++)
		{
			local_node->m_uiUdfSizeTot += CalcNodeSizeTotal(*it);
		}

		return local_node->m_uiUdfSizeTot;
	}

	/*
		FIXME: This function uses recursion, it should be converted to an
		iterative function to avoid the risk of running out of memory.
	*/
	ckcore::tuint64 UdfWriter::CalcNodeLinksTotal(FileTreeNode *local_node)
	{
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->m_Children.begin(); it !=
			local_node->m_Children.end(); it++)
		{
			local_node->m_uiUdfLinkTot += CalcNodeLinksTotal(*it);
		}

		return (local_node->file_flags_ & FileTreeNode::FLAG_DIRECTORY) ? 1 : 0;
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

	bool UdfWriter::WriteLocalParitionDir(std::deque<FileTreeNode *> &dir_node_queue,
										  FileTreeNode *local_node,unsigned long &cur_part_sec,
										  ckcore::tuint64 &unique_ident)
	{
		unsigned long entry_sec = cur_part_sec++;
		unsigned long ident_sec = cur_part_sec;	// On folders the identifiers will follow immediately.

		// Calculate the size of all identifiers.
		ckcore::tuint64 tot_ident_size = 0;
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->m_Children.begin(); it !=
			local_node->m_Children.end(); it++)
		{
			tot_ident_size += udf_.CalcFileIdentSize((*it)->m_FileName.c_str());
		}

		// Don't forget to add the '..' item to the total.
		tot_ident_size += udf_.CalcFileIdentParentSize();

		unsigned long next_entry_sec = ident_sec + bytes_to_sec(tot_ident_size);

		// Get file modified dates.
		struct tm access_time,modify_time,create_time;
		if (!ckcore::Directory::Time(local_node->file_path_.c_str(),access_time,modify_time,create_time))
			access_time = modify_time = create_time = create_time_;

		// The current folder entry.
		if (!udf_.WriteFileEntry(out_stream_,entry_sec,true,(unsigned short)local_node->m_uiUdfLinkTot + 1,
			unique_ident,ident_sec,tot_ident_size,access_time,modify_time,create_time))
		{
			return false;
		}

		// Unique identifiers 0-15 are reserved for Macintosh implementations.
		if (unique_ident == 0)
			unique_ident = 16;
		else
			unique_ident++;

		// The '..' item.
		unsigned long parent_entry_sec = local_node->GetParent() == NULL ? entry_sec : local_node->GetParent()->m_ulUdfPartLoc;
		if (!udf_.WriteFileIdentParent(out_stream_,cur_part_sec,parent_entry_sec))
			return false;

		// Keep track on how many bytes we have in our sector.
		unsigned long sec_bytes = udf_.CalcFileIdentParentSize();

		std::vector<FileTreeNode *> tmp_stack;
		for (it = local_node->m_Children.begin(); it !=
			local_node->m_Children.end(); it++)
		{
			// Push the item to the temporary stack.
			tmp_stack.push_back(*it);

			if ((*it)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				if (!udf_.WriteFileIdent(out_stream_,cur_part_sec,next_entry_sec,true,(*it)->m_FileName.c_str()))
					return false;

				(*it)->m_ulUdfPartLoc = next_entry_sec;	// Remember where this entry was stored.
				next_entry_sec += (unsigned long)(*it)->m_uiUdfSizeTot;
			}
			else
			{
				if (!udf_.WriteFileIdent(out_stream_,cur_part_sec,next_entry_sec,false,(*it)->m_FileName.c_str()))
					return false;

				(*it)->m_ulUdfPartLoc = next_entry_sec;	// Remember where this entry was stored.
				next_entry_sec += (unsigned long)(*it)->m_uiUdfSizeTot;
			}

			sec_bytes += udf_.CalcFileIdentSize((*it)->m_FileName.c_str());
			if (sec_bytes >= UDF_SECTOR_SIZE)
			{
				cur_part_sec++;
				sec_bytes -= UDF_SECTOR_SIZE;
			}
		}

		// Insert the elements from the temporary stack into the queue.
		std::vector<FileTreeNode *>::reverse_iterator it_stack;
		for (it_stack = tmp_stack.rbegin(); it_stack != tmp_stack.rend(); it_stack++)
			dir_node_queue.push_front(*it_stack);

		// Pad to the next sector.
		out_stream_.PadSector();

		if (sec_bytes > 0)
			cur_part_sec++;

		return true;
	}

	bool UdfWriter::WritePartitionEntries(FileTree &file_tree)
	{
		// We start at partition sector 1, sector 0 is the parition anchor descriptor.
		unsigned long cur_part_sec = 1;

		// We start with unique identifier 0 (which is reserved for root) and
		// increase it for every file or folder added.
		ckcore::tuint64 unique_ident = 0;

		FileTreeNode *cur_node = file_tree.GetRoot();
		cur_node->m_ulUdfPartLoc = cur_part_sec;

		std::deque<FileTreeNode *> dir_node_stack;
		if (!WriteLocalParitionDir(dir_node_stack,cur_node,cur_part_sec,unique_ident))
			return false;

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack.front();
			dir_node_stack.pop_front();

#ifdef _DEBUG
			if (cur_node->m_ulUdfPartLoc != cur_part_sec)
			{
				log_.PrintLine(ckT("Invalid location for \"%s\" in UDF file system. Proposed position %u verus actual position %u."),
					cur_node->file_path_.c_str(),cur_node->m_ulUdfPartLoc,cur_part_sec);
			}
#endif

			if (cur_node->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				if (!WriteLocalParitionDir(dir_node_stack,cur_node,cur_part_sec,unique_ident))
					return false;
			}
			else
			{
				// Get file modified dates.
				struct tm access_time,modify_time,create_time;
				if (use_file_times_ && !ckcore::File::Time(cur_node->file_path_.c_str(),access_time,modify_time,create_time))
					access_time = modify_time = create_time = create_time_;

				if (!udf_.WriteFileEntry(out_stream_,cur_part_sec++,false,1,
					unique_ident,(unsigned long)cur_node->m_uiDataPosNormal - 257,cur_node->file_size_,
					access_time,modify_time,create_time))
				{
					return false;
				}

				// Unique identifiers 0-15 are reserved for Macintosh implementations.
				if (unique_ident == 0)
					unique_ident = UDF_UNIQUEIDENT_MIN;
				else
					unique_ident++;
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
		part_len_ = CalcParitionLength(file_tree);
		sec_manager_.AllocateSectors(this,SR_FILESETCONTENTS,part_len_);

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
		if (part_len_ == 0)
		{
			log_.PrintLine(ckT("  Error: Cannot write UDF parition since no space has been reserved for it."));
			return RESULT_FAIL;
		}

		if (part_len_ > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: UDF partition is too large (%I64u sectors)."),part_len_);
#else
			log_.PrintLine(ckT("  Error: UDF partition is too large (%llu sectors)."),part_len_);
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
		char tmp[1] = { 0 };

		// Parition size = the partition size calculated above + the set descriptor + the data length.
		unsigned long udf_cur_sec = (unsigned long)sec_manager_.GetStart(this,SR_MAINDESCRIPTORS);
		unsigned long udf_part_len = (unsigned long)part_len_ + 1 + (unsigned long)sec_manager_.GetDataLength();

		// Assign a unique identifier that's larger than any unique identifier of a
		// file entry + 16 for the reserved numbers.
		ckcore::tuint64 unique_ident = file_tree.GetDirCount() + file_tree.GetFileCount() + 1 + UDF_UNIQUEIDENT_MIN;

		tudf_extent_ad integrity_seq_extent;
		integrity_seq_extent.extent_len = UDF_SECTOR_SIZE;
		integrity_seq_extent.extent_loc = udf_cur_sec;

		if (!udf_.WriteVolDescLogIntegrity(out_stream_,udf_cur_sec,file_tree.GetFileCount(),
			file_tree.GetDirCount() + 1,udf_part_len,unique_ident,create_time_))
		{
			log_.PrintLine(ckT("  Error: Failed to write UDF logical integrity descriptor."));
			return RESULT_FAIL;
		}
		udf_cur_sec++;			

		// 6 volume descriptors. But minimum length is 16 so we pad with empty sectors.
		voldesc_seqextent_main_.extent_len = voldesc_seqextent_rsrv_.extent_len = 16 * ISO9660_SECTOR_SIZE;

		// We should write the set of volume descriptors twice.
		for (unsigned int i = 0; i < 2; i++)
		{
			// Remember the start of the volume descriptors.
			if (i == 0)
				voldesc_seqextent_main_.extent_loc = udf_cur_sec;
			else
				voldesc_seqextent_rsrv_.extent_loc = udf_cur_sec;

			unsigned long voldesc_seqnum = 0;
			if (!udf_.WriteVolDescPrimary(out_stream_,voldesc_seqnum++,udf_cur_sec,create_time_))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF primary volume descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!udf_.WriteVolDescImplUse(out_stream_,voldesc_seqnum++,udf_cur_sec))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF implementation use descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!udf_.WriteVolDescPartition(out_stream_,voldesc_seqnum++,udf_cur_sec,257,udf_part_len))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF partition descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!udf_.WriteVolDescLogical(out_stream_,voldesc_seqnum++,udf_cur_sec,integrity_seq_extent))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF logical partition descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!udf_.WriteVolDescUnalloc(out_stream_,voldesc_seqnum++,udf_cur_sec))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF unallocated space descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!udf_.WriteVolDescTerm(out_stream_,udf_cur_sec))
			{
				log_.PrintLine(ckT("  Error: Failed to write UDF descriptor terminator."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			// According to UDF 1.02 standard each volume descriptor
			// sequence extent must contain atleast 16 sectors. Because of
			// this we need to add 10 empty sectors.
			for (int j = 0; j < 10; j++)
			{
				for (unsigned long i = 0; i < ISO9660_SECTOR_SIZE; i++)
					out_stream_.Write(tmp,1);
				udf_cur_sec++;
			}
		}

		// Allocate everything until sector 256 with empty sectors.
		while (udf_cur_sec < 256)
		{
			for (unsigned long i = 0; i < ISO9660_SECTOR_SIZE; i++)
				out_stream_.Write(tmp,1);
			udf_cur_sec++;
		}

		// At sector 256 write the first anchor volume descriptor pointer.
		if (!udf_.WriteAnchorVolDescPtr(out_stream_,udf_cur_sec,voldesc_seqextent_main_,voldesc_seqextent_rsrv_))
		{
			log_.PrintLine(ckT("  Error: Failed to write anchor volume descriptor pointer."));
			return RESULT_FAIL;
		}
		udf_cur_sec++;

		// The file set descriptor is the first entry in the partition, hence the logical block address 0.
		// The root is located directly after this descriptor, hence the location 1.
		if (!udf_.WriteFileSetDesc(out_stream_,0,1,create_time_))
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
		ckcore::tuint64 last_data_sec = sec_manager_.GetDataStart() +
			sec_manager_.GetDataLength();
		if (last_data_sec > 0xFFFFFFFF)
		{
#ifdef _WINDOWS
			log_.PrintLine(ckT("  Error: File data too large, last data sector is %I64d."),last_data_sec);
#else
			log_.PrintLine(ckT("  Error: File data too large, last data sector is %lld."),last_data_sec);
#endif
			return RESULT_FAIL;
		}

		// Finally write the 2nd and last anchor volume descriptor pointer.
		if (!udf_.WriteAnchorVolDescPtr(out_stream_,(unsigned long)last_data_sec,
			voldesc_seqextent_main_,voldesc_seqextent_rsrv_))
		{
			log_.PrintLine(ckT("  Error: Failed to write anchor volume descriptor pointer."));
			return RESULT_FAIL;
		}

		out_stream_.PadSector();
		return RESULT_OK;
	}
};
