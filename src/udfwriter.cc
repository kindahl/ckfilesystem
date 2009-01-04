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

#include <time.h>
#include <string.h>
#include <ckcore/file.hh>
#include <ckcore/directory.hh>
#include "ckfilesystem/iso9660.hh"
#include "ckfilesystem/util.hh"
#include "ckfilesystem/udfwriter.hh"

namespace ckfilesystem
{
	UdfWriter::UdfWriter(ckcore::Log &log,SectorOutStream &out_stream,SectorManager &sec_manager,
		FileSystem &file_sys,bool use_file_times) :
		log_(log),out_stream_(out_stream),sec_manager_(sec_manager),
		file_sys_(file_sys),use_file_times_(use_file_times),part_len_(0)
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

	void UdfWriter::calc_local_node_lengths(std::vector<FileTreeNode *> &dir_node_stack,
		                                    FileTreeNode *local_node)
	{
		local_node->udf_size_ = 0;
		local_node->udf_size_ += util::bytes_to_sec(file_sys_.udf_.calc_file_entry_size());
		local_node->udf_size_ += util::bytes_to_sec(calc_ident_size(local_node));
		local_node->udf_size_tot_ = local_node->udf_size_;

		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->children_.begin(); it !=
			local_node->children_.end(); it++)
		{
			if ((*it)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
				dir_node_stack.push_back(*it);
			else
			{
				(*it)->udf_size_ = util::bytes_to_sec(file_sys_.udf_.calc_file_entry_size());
				(*it)->udf_size_tot_ = (*it)->udf_size_;
			}
		}
	}

	void UdfWriter::calc_node_lengths(FileTree &file_tree)
	{
		FileTreeNode *cur_node = file_tree.get_root();

		std::vector<FileTreeNode *> dir_node_stack;
		calc_local_node_lengths(dir_node_stack,cur_node);

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack[dir_node_stack.size() - 1];
			dir_node_stack.pop_back();

			calc_local_node_lengths(dir_node_stack,cur_node);
		}
	}

	ckcore::tuint64 UdfWriter::calc_ident_size(FileTreeNode *local_node)
	{
		ckcore::tuint64 tot_ident_size = 0;
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->children_.begin(); it !=
			local_node->children_.end(); it++)
		{
			tot_ident_size += file_sys_.udf_.calc_file_ident_size((*it)->file_name_.c_str());
		}

		// Don't forget to add the '..' item to the total.
		tot_ident_size += file_sys_.udf_.calc_file_ident_parent_size();
		return tot_ident_size;
	}

	/*
		FIXME: This function uses recursion, it should be converted to an
		iterative function to avoid the risk of running out of memory.
	*/
	ckcore::tuint64 UdfWriter::calc_node_size_total(FileTreeNode *local_node)
	{
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->children_.begin(); it !=
			local_node->children_.end(); it++)
		{
			local_node->udf_size_tot_ += calc_node_size_total(*it);
		}

		return local_node->udf_size_tot_;
	}

	/*
		FIXME: This function uses recursion, it should be converted to an
		iterative function to avoid the risk of running out of memory.
	*/
	ckcore::tuint64 UdfWriter::calc_node_links_total(FileTreeNode *local_node)
	{
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->children_.begin(); it !=
			local_node->children_.end(); it++)
		{
			local_node->udf_link_tot_ += calc_node_links_total(*it);
		}

		return (local_node->file_flags_ & FileTreeNode::FLAG_DIRECTORY) ? 1 : 0;
	}

	/**
		Calculates the number of bytes needed to store the UDF partition.
	*/
	ckcore::tuint64 UdfWriter::calc_partition_len(FileTree &file_tree)
	{
		// First wee need to calculate the individual sizes of each records.
		calc_node_lengths(file_tree);

		// Calculate the number of directory links associated with each directory node.
		calc_node_links_total(file_tree.get_root());

		// The update the compelte tree (unfortunately recursively).
		return calc_node_size_total(file_tree.get_root());
	}

	bool UdfWriter::write_local_partition_dir(std::deque<FileTreeNode *> &dir_node_queue,
										      FileTreeNode *local_node,ckcore::tuint32 &cur_part_sec,
										      ckcore::tuint64 &unique_ident)
	{
		ckcore::tuint32 entry_sec = cur_part_sec++;
		ckcore::tuint32 ident_sec = cur_part_sec;	// On folders the identifiers will follow immediately.

		// Calculate the size of all identifiers.
		ckcore::tuint64 tot_ident_size = 0;
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->children_.begin(); it !=
			local_node->children_.end(); it++)
		{
			tot_ident_size += file_sys_.udf_.calc_file_ident_size((*it)->file_name_.c_str());
		}

		// Don't forget to add the '..' item to the total.
		tot_ident_size += file_sys_.udf_.calc_file_ident_parent_size();

		ckcore::tuint32 next_entry_sec = ident_sec + util::bytes_to_sec(tot_ident_size);

		// Get file modified dates.
		struct tm access_time,modify_time,create_time;
		if (!ckcore::Directory::time(local_node->file_path_.c_str(),access_time,modify_time,create_time))
			access_time = modify_time = create_time = create_time_;

		// The current folder entry.
		if (!file_sys_.udf_.write_file_entry(out_stream_,entry_sec,true,(ckcore::tuint16)local_node->udf_link_tot_ + 1,
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
		ckcore::tuint32 parent_entry_sec = local_node->get_parent() == NULL ? entry_sec : local_node->get_parent()->udf_part_loc_;
		if (!file_sys_.udf_.write_file_ident_parent(out_stream_,cur_part_sec,parent_entry_sec))
			return false;

		// Keep track on how many bytes we have in our sector.
		ckcore::tuint32 sec_bytes = file_sys_.udf_.calc_file_ident_parent_size();

		std::vector<FileTreeNode *> tmp_stack;
		for (it = local_node->children_.begin(); it !=
			local_node->children_.end(); it++)
		{
			// Push the item to the temporary stack.
			tmp_stack.push_back(*it);

			if ((*it)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				if (!file_sys_.udf_.write_file_ident(out_stream_,cur_part_sec,next_entry_sec,true,(*it)->file_name_.c_str()))
					return false;

				(*it)->udf_part_loc_ = next_entry_sec;	// Remember where this entry was stored.
				next_entry_sec += (ckcore::tuint32)(*it)->udf_size_tot_;
			}
			else
			{
				if (!file_sys_.udf_.write_file_ident(out_stream_,cur_part_sec,next_entry_sec,false,(*it)->file_name_.c_str()))
					return false;

				(*it)->udf_part_loc_ = next_entry_sec;	// Remember where this entry was stored.
				next_entry_sec += (ckcore::tuint32)(*it)->udf_size_tot_;
			}

			sec_bytes += file_sys_.udf_.calc_file_ident_size((*it)->file_name_.c_str());
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
		out_stream_.pad_sector();

		if (sec_bytes > 0)
			cur_part_sec++;

		return true;
	}

	bool UdfWriter::write_partition_entries(FileTree &file_tree)
	{
		// We start at partition sector 1, sector 0 is the parition anchor descriptor.
		ckcore::tuint32 cur_part_sec = 1;

		// We start with unique identifier 0 (which is reserved for root) and
		// increase it for every file or folder added.
		ckcore::tuint64 unique_ident = 0;

		FileTreeNode *cur_node = file_tree.get_root();
		cur_node->udf_part_loc_ = cur_part_sec;

		std::deque<FileTreeNode *> dir_node_stack;
		if (!write_local_partition_dir(dir_node_stack,cur_node,cur_part_sec,unique_ident))
			return false;

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack.front();
			dir_node_stack.pop_front();

#ifdef _DEBUG
			if (cur_node->udf_part_loc_ != cur_part_sec)
			{
				log_.print_line(ckT("Invalid location for \"%s\" in UDF file system. Proposed position %u verus actual position %u."),
					cur_node->file_path_.c_str(),cur_node->udf_part_loc_,cur_part_sec);
			}
#endif

			if (cur_node->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				if (!write_local_partition_dir(dir_node_stack,cur_node,cur_part_sec,unique_ident))
					return false;
			}
			else
			{
				// Get file modified dates.
				struct tm access_time,modify_time,create_time;
				if (use_file_times_ && !ckcore::File::time(cur_node->file_path_.c_str(),access_time,modify_time,create_time))
					access_time = modify_time = create_time = create_time_;

				if (!file_sys_.udf_.write_file_entry(out_stream_,cur_part_sec++,false,1,
					unique_ident,(ckcore::tuint32)cur_node->data_pos_normal_ - 257,cur_node->file_size_,
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

	int UdfWriter::alloc_header()
	{
		sec_manager_.alloc_bytes(this,SR_INITIALDESCRIPTORS,file_sys_.udf_.get_vol_desc_initial_size());
		return RESULT_OK;
	}

	int UdfWriter::alloc_partition(FileTree &file_tree)
	{
		// Allocate everything up to sector 258.
		sec_manager_.alloc_sectors(this,SR_MAINDESCRIPTORS,258 - sec_manager_.get_next_free());

		// Allocate memory for the file set contents.
		part_len_ = calc_partition_len(file_tree);
		sec_manager_.alloc_sectors(this,SR_FILESETCONTENTS,part_len_);

		return RESULT_OK;
	}

	int UdfWriter::write_header()
	{
		if (!file_sys_.udf_.write_vol_desc_initial(out_stream_))
		{
			log_.print_line(ckT("  Error: Failed to write initial UDF volume descriptors."));
			return RESULT_FAIL;
		}

		return RESULT_OK;
	}

	int UdfWriter::write_partition(FileTree &file_tree)
	{
		if (part_len_ == 0)
		{
			log_.print_line(ckT("  Error: Cannot write UDF parition since no space has been reserved for it."));
			return RESULT_FAIL;
		}

		if (part_len_ > 0xffffffff)
		{
#ifdef _WINDOWS
			log_.print_line(ckT("  Error: UDF partition is too large (%I64u sectors)."),part_len_);
#else
			log_.print_line(ckT("  Error: UDF partition is too large (%llu sectors)."),part_len_);
#endif
			return RESULT_FAIL;
		}

		if (sec_manager_.get_start(this,SR_MAINDESCRIPTORS) > 0xffffffff)
		{
#ifdef _WINDOWS
			log_.print_line(ckT("  Error: Error during sector space allocation. Start of UDF main descriptors %I64d."),
#else
			log_.print_line(ckT("  Error: Error during sector space allocation. Start of UDF main descriptors %lld."),
#endif
				sec_manager_.get_start(this,SR_MAINDESCRIPTORS));
			return RESULT_FAIL;
		}

		if (sec_manager_.get_data_length() > 0xffffffff)
		{
#ifdef _WINDOWS
			log_.print_line(ckT("  Error: File data too large (%I64d sectors) for UDF."),sec_manager_.get_data_length());
#else
			log_.print_line(ckT("  Error: File data too large (%lld sectors) for UDF."),sec_manager_.get_data_length());
#endif
			return RESULT_FAIL;
		}

		// Used for padding.
		char tmp[1] = { 0 };

		// Parition size = the partition size calculated above + the set descriptor + the data length.
		ckcore::tuint32 udf_cur_sec = (ckcore::tuint32)sec_manager_.get_start(this,SR_MAINDESCRIPTORS);
		ckcore::tuint32 udf_part_len = (ckcore::tuint32)part_len_ + 1 + (ckcore::tuint32)sec_manager_.get_data_length();

		// Assign a unique identifier that's larger than any unique identifier of a
		// file entry + 16 for the reserved numbers.
		ckcore::tuint64 unique_ident = file_tree.get_dir_count() + file_tree.get_file_count() + 1 + UDF_UNIQUEIDENT_MIN;

		tudf_extent_ad integrity_seq_extent;
		integrity_seq_extent.extent_len = UDF_SECTOR_SIZE;
		integrity_seq_extent.extent_loc = udf_cur_sec;

		if (!file_sys_.udf_.write_vol_desc_log_integrity(out_stream_,udf_cur_sec,file_tree.get_file_count(),
			file_tree.get_dir_count() + 1,udf_part_len,unique_ident,create_time_))
		{
			log_.print_line(ckT("  Error: Failed to write UDF logical integrity descriptor."));
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

			ckcore::tuint32 voldesc_seqnum = 0;
			if (!file_sys_.udf_.write_vol_desc_primary(out_stream_,voldesc_seqnum++,udf_cur_sec,create_time_))
			{
				log_.print_line(ckT("  Error: Failed to write UDF primary volume descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!file_sys_.udf_.write_vol_desc_impl_use(out_stream_,voldesc_seqnum++,udf_cur_sec))
			{
				log_.print_line(ckT("  Error: Failed to write UDF implementation use descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!file_sys_.udf_.write_vol_desc_partition(out_stream_,voldesc_seqnum++,udf_cur_sec,257,udf_part_len))
			{
				log_.print_line(ckT("  Error: Failed to write UDF partition descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!file_sys_.udf_.write_vol_desc_logical(out_stream_,voldesc_seqnum++,udf_cur_sec,integrity_seq_extent))
			{
				log_.print_line(ckT("  Error: Failed to write UDF logical partition descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!file_sys_.udf_.write_vol_desc_unalloc(out_stream_,voldesc_seqnum++,udf_cur_sec))
			{
				log_.print_line(ckT("  Error: Failed to write UDF unallocated space descriptor."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			if (!file_sys_.udf_.write_vol_desc_term(out_stream_,udf_cur_sec))
			{
				log_.print_line(ckT("  Error: Failed to write UDF descriptor terminator."));
				return RESULT_FAIL;
			}
			udf_cur_sec++;

			// According to UDF 1.02 standard each volume descriptor
			// sequence extent must contain atleast 16 sectors. Because of
			// this we need to add 10 empty sectors.
			for (int j = 0; j < 10; j++)
			{
				for (ckcore::tuint32 i = 0; i < ISO9660_SECTOR_SIZE; i++)
					out_stream_.write(tmp,1);
				udf_cur_sec++;
			}
		}

		// Allocate everything until sector 256 with empty sectors.
		while (udf_cur_sec < 256)
		{
			for (ckcore::tuint32 i = 0; i < ISO9660_SECTOR_SIZE; i++)
				out_stream_.write(tmp,1);
			udf_cur_sec++;
		}

		// At sector 256 write the first anchor volume descriptor pointer.
		if (!file_sys_.udf_.write_anchor_vol_desc_ptr(out_stream_,udf_cur_sec,voldesc_seqextent_main_,voldesc_seqextent_rsrv_))
		{
			log_.print_line(ckT("  Error: Failed to write anchor volume descriptor pointer."));
			return RESULT_FAIL;
		}
		udf_cur_sec++;

		// The file set descriptor is the first entry in the partition, hence the logical block address 0.
		// The root is located directly after this descriptor, hence the location 1.
		if (!file_sys_.udf_.write_file_set_desc(out_stream_,0,1,create_time_))
		{
			log_.print_line(ckT("  Error: Failed to write file set descriptor."));
			return RESULT_FAIL;
		}

		if (!write_partition_entries(file_tree))
		{
			log_.print_line(ckT("  Error: Failed to write file UDF partition."));
			return RESULT_FAIL;
		}

		return RESULT_OK;
	}

	int UdfWriter::write_tail()
	{
		ckcore::tuint64 last_data_sec = sec_manager_.get_data_start() +
			sec_manager_.get_data_length();
		if (last_data_sec > 0xffffffff)
		{
#ifdef _WINDOWS
			log_.print_line(ckT("  Error: File data too large, last data sector is %I64d."),last_data_sec);
#else
			log_.print_line(ckT("  Error: File data too large, last data sector is %lld."),last_data_sec);
#endif
			return RESULT_FAIL;
		}

		// Finally write the 2nd and last anchor volume descriptor pointer.
		if (!file_sys_.udf_.write_anchor_vol_desc_ptr(out_stream_,(ckcore::tuint32)last_data_sec,
			voldesc_seqextent_main_,voldesc_seqextent_rsrv_))
		{
			log_.print_line(ckT("  Error: Failed to write anchor volume descriptor pointer."));
			return RESULT_FAIL;
		}

		out_stream_.pad_sector();
		return RESULT_OK;
	}
};
