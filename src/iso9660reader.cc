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
	Iso9660Reader::Iso9660Reader(ckcore::Log &log) :
		log_(log),root_node_(NULL)
	{
	}

	Iso9660Reader::~Iso9660Reader()
	{
		if (root_node_ != NULL)
		{
			delete root_node_;
			root_node_ = NULL;
		}
	}

	/**
		Reads an entire directory entry.
	 */
	bool Iso9660Reader::read_dir_entry(ckcore::InStream &in_stream,
									   std::vector<Iso9660TreeNode *> &dir_entries,
									   Iso9660TreeNode *parent_node,bool joliet)
	{
		// Search to the extent location.
		in_stream.seek(parent_node->extent_loc_ * ISO9660_SECTOR_SIZE,ckcore::InStream::ckSTREAM_BEGIN);

		unsigned char file_name_buf[ISO9660WRITER_FILENAME_BUFFER_SIZE];
		ckcore::tchar file_name[(ISO9660WRITER_FILENAME_BUFFER_SIZE / sizeof(ckcore::tchar)) + 1];

		// Skip the '.' and '..' entries.
		unsigned long read = 0;
		ckcore::tint64 processed = 0;

		for (unsigned int i = 0; i < 2; i++)
		{
			unsigned char sysdir_rec_len = 0;
			processed = in_stream.read(&sysdir_rec_len,1);
			if (processed == -1)
			{
				log_.print_line(ckT("Error: Unable to read system directory record length."));
				return false;
			}
			if (processed != 1)
			{
				log_.print_line(ckT("Error: Unable to read system directory record length (size mismatch)."));
				return false;
			}

			in_stream.seek(sysdir_rec_len - 1,ckcore::InStream::ckSTREAM_CURRENT);
			read += sysdir_rec_len;
		}

		tiso_dir_record dr;

		// Read all other records.
		while (read < parent_node->extent_len_)
		{
			// Check if we can read further.
			if (read + sizeof(tiso_dir_record) > parent_node->extent_len_)
				break;

			unsigned long dir_rec_processed = 0;

			memset(&dr,0,sizeof(tiso_dir_record));
			processed = in_stream.read(&dr,sizeof(tiso_dir_record) - 1);
			if (processed == -1)
			{
				log_.print_line(ckT("Error: Unable to read directory record."));
				return false;
			}
			if (processed != sizeof(tiso_dir_record) - 1)
			{
				log_.print_line(ckT("Error: Unable to read directory record (size mismatch: %u vs. %u)."),
					(unsigned long)processed,sizeof(tiso_dir_record) - 1);
				return false;
			}
			dir_rec_processed += (unsigned long)processed;

			if (dr.file_ident_len > ISO9660WRITER_FILENAME_BUFFER_SIZE)
			{
				log_.print_line(ckT("Error: Directory record file identifier is too large: %u bytes."),
					dr.file_ident_len);
				return false;
			}

			// Read identifier.
			processed = in_stream.read(file_name_buf,dr.file_ident_len);
			if (processed == -1)
			{
				log_.print_line(ckT("Error: Unable to read directory record file identifier."));
				return false;
			}
			if (processed != dr.file_ident_len)
			{
				log_.print_line(ckT("Error: Unable to read directory record file identifier (size mismatch)."));
				return false;
			}
			dir_rec_processed += (unsigned long)processed;

			// Convert identifier.
			if (joliet)
			{
#ifdef _UNICODE
				wchar_t *utf_file_name = file_name;
#else
				wchar_t utf_file_name[(ISO9660WRITER_FILENAME_BUFFER_SIZE >> 1) + 1];
#endif
				unsigned char file_name_len = dr.file_ident_len >> 1;
				unsigned char file_name_pos = 0;

				for (unsigned int i = 0; i < file_name_len; i++)
				{
					utf_file_name[i]  = file_name_buf[file_name_pos++] << 8;
					utf_file_name[i] |= file_name_buf[file_name_pos++];
				}

				utf_file_name[file_name_len] = '\0';

#ifndef _UNICODE
                ckcore::string::utf16_to_ansi(utf_file_name,file_name,sizeof(file_name));
#endif
			}
			else
			{
#ifdef _UNICODE
				ckcore::string::ansi_to_utf16((const char *)file_name_buf,file_name,sizeof(file_name) / sizeof(ckcore::tchar));
#else
				memcpy(file_name,file_name_buf,dr.file_ident_len);
#endif
				file_name[dr.file_ident_len] = '\0';
			}

			// Remove any version information.
			unsigned char len = joliet ? dr.file_ident_len >> 1 :
				dr.file_ident_len;
			if (file_name[len - 2] == ';')
				file_name[len - 2] = '\0';

			//log_.print_line(ckT("  %s: %u"),file_name,read733(dr.extent_loc));

			Iso9660TreeNode *new_node = new Iso9660TreeNode(parent_node,file_name,
					read733(dr.extent_loc),read733(dr.data_len),
					read723(dr.volseq_num),dr.file_flags,
					dr.file_unit_size,dr.interleave_gap_size,dr.rec_timestamp);

			if (dr.file_flags & DIRRECORD_FILEFLAG_DIRECTORY)
				dir_entries.push_back(new_node);

			parent_node->children_.push_back(new_node);

			// Skip any extra data.
			if (dr.dir_record_len - dir_rec_processed > 0)
			{
				in_stream.seek(dr.dir_record_len - dir_rec_processed,ckcore::InStream::ckSTREAM_CURRENT);
				dir_rec_processed = dr.dir_record_len;
			}

			read += dir_rec_processed;

			// Skip any extended attribute record.
			in_stream.seek(dr.ext_attr_record_len,ckcore::InStream::ckSTREAM_CURRENT);
			read += dr.ext_attr_record_len;

			// Ignore any zeroes.
			unsigned char next_byte;
			processed = in_stream.read(&next_byte,1);
			if (processed == -1)
			{
				log_.print_line(ckT("Error: Unable to read through zeroes."));
				return false;
			}
			if (processed != 1)
			{
				log_.print_line(ckT("Error: Unable to read through zeroes (size mismatch)."));
				return false;
			}

			while (next_byte == 0)
			{
				read++;

				processed = in_stream.read(&next_byte,1);
				if (processed == -1)
				{
					log_.print_line(ckT("Error: Unable to read through zeroes."));
					return false;
				}
				if (processed != 1)
				{
					log_.print_line(ckT("Error: Unable to read through zeroes (size mismatch)."));
					return false;
				}
			}

			in_stream.seek(parent_node->extent_loc_ * ISO9660_SECTOR_SIZE + read,ckcore::InStream::ckSTREAM_BEGIN);
		}

		return true;
	}

	bool Iso9660Reader::read(ckcore::InStream &in_stream,unsigned long start_sec)
	{
		log_.print_line(ckT("Iso9660Reader::Read"));

		// Seek to sector 16.
		in_stream.seek(ISO9660_SECTOR_SIZE * (16 + start_sec),ckcore::InStream::ckSTREAM_BEGIN);
		if (in_stream.end())
		{
			log_.print_line(ckT("  Error: Invalid ISO9660 file system."));
			return false;
		}

		ckcore::tint64 processed = 0;

		// Read the primary volume descriptor.
		tiso_voldesc_primary voldesc_prim;
		memset(&voldesc_prim,0,sizeof(tiso_voldesc_primary));
		processed = in_stream.read(&voldesc_prim,sizeof(tiso_voldesc_primary));
		if (processed == -1)
		{
			log_.print_line(ckT("  Error: Unable to read ISO9660 file system."));
			return false;
		}
		if (processed != sizeof(tiso_voldesc_primary))
		{
			log_.print_line(ckT("  Error: Unable to read ISO9660 primary volume descriptor."));
			return false;
		}

		// Validate primary volume descriptor.
		if (voldesc_prim.type != VOLDESCTYPE_PRIM_VOL_DESC)
		{
			log_.print_line(ckT("  Error: Primary volume decsriptor not found at sector 16."));
			return false;
		}

		if (voldesc_prim.version != 1)
		{
			log_.print_line(ckT("  Error: Bad primary volume descriptor version."));
			return false;
		}

		if (voldesc_prim.file_struct_ver != 1)
		{
			log_.print_line(ckT("  Error: Bad primary volume descriptor structure version."));
			return false;
		}

		if (memcmp(voldesc_prim.ident,iso_ident_cd,sizeof(voldesc_prim.ident)))
		{
			log_.print_line(ckT("  Error: Bad primary volume descriptor identifer."));
			return false;
		}

		// Search all other volume descriptors for a Joliet descriptor (we only search 99 maximum).
		tiso_voldesc_suppl voldesc_suppl;
		bool joliet = false;
		for (unsigned int i = 0; i < 99; i++)
		{
			memset(&voldesc_suppl,0,sizeof(tiso_voldesc_suppl));
			processed = in_stream.read(&voldesc_suppl,sizeof(tiso_voldesc_suppl));
			if (processed == -1)
			{
				log_.print_line(ckT("  Error: Unable to read ISO9660 file system."));
				return false;
			}
			if (processed != sizeof(tiso_voldesc_suppl))
			{
				log_.print_line(ckT("  Error: Unable to read additional ISO9660 volume descriptor."));
				return false;
			}

			// Check if we found the terminator.
			if (voldesc_suppl.type == VOLDESCTYPE_VOL_DESC_SET_TERM)
				break;

			// Check if we found a supplementary volume descriptor.
			if (voldesc_suppl.type == VOLDESCTYPE_SUPPL_VOL_DESC)
			{
				// Check if Joliet.
				if (voldesc_suppl.esc_sec[0] == 0x25 &&
					voldesc_suppl.esc_sec[1] == 0x2F)
				{
					if (voldesc_suppl.esc_sec[2] == 0x45 ||
						voldesc_suppl.esc_sec[2] == 0x43 ||
						voldesc_suppl.esc_sec[2] == 0x40)
					{
						log_.print_line(ckT("  Found Joliet file system extension."));
						joliet = true;
						break;
					}
				}
			}
		}

		// Obtain positions of interest.
		unsigned long root_extent_loc;
		unsigned long root_extent_len;

		if (joliet)
		{
			root_extent_loc = read733(voldesc_suppl.root_dir_record.extent_loc);
			root_extent_len = read733(voldesc_suppl.root_dir_record.data_len);
		}
		else
		{
			root_extent_loc = read733(voldesc_prim.root_dir_record.extent_loc);
			root_extent_len = read733(voldesc_prim.root_dir_record.data_len);
		}

		log_.print_line(ckT("  Location of root directory extent: %u."),root_extent_loc);
		log_.print_line(ckT("  Length of root directory extent: %u."),root_extent_len);

		if (root_node_ != NULL)
			delete root_node_;

		root_node_ = new Iso9660TreeNode(NULL,NULL,root_extent_loc,root_extent_len,
			read723(voldesc_suppl.root_dir_record.volseq_num),voldesc_suppl.root_dir_record.file_flags,
			voldesc_suppl.root_dir_record.file_unit_size,voldesc_suppl.root_dir_record.interleave_gap_size,
			voldesc_suppl.root_dir_record.rec_timestamp);

		std::vector<Iso9660TreeNode *> dir_entries;
		if (!read_dir_entry(in_stream,dir_entries,root_node_,joliet))
		{
			log_.print_line(ckT("  Error: Failed to read directory entry at sector: %u."),root_extent_loc);
			return false;
		}

		while (dir_entries.size() > 0)
		{
			Iso9660TreeNode *parent_node = dir_entries.back();
			dir_entries.pop_back();

			if (!read_dir_entry(in_stream,dir_entries,parent_node,joliet))
			{
				log_.print_line(ckT("  Error: Failed to read directory entry at sector: %u."),parent_node->extent_loc_);
				return false;
			}
		}

		return true;
	}

	#ifdef _DEBUG
	void Iso9660Reader::print_local_tree(std::vector<std::pair<Iso9660TreeNode *,int> > &dir_node_stack,
									     Iso9660TreeNode *local_node,int indent)
	{
		std::vector<Iso9660TreeNode *>::const_iterator it_file;
		for (it_file = local_node->children_.begin(); it_file !=
			local_node->children_.end(); it_file++)
		{
			if ((*it_file)->file_flags_ & DIRRECORD_FILEFLAG_DIRECTORY)
			{
				dir_node_stack.push_back(std::make_pair(*it_file,indent));
			}
			else
			{
				for (int i = 0; i < indent; i++)
					log_.Print(ckT(" "));

				log_.Print(ckT("<f>"));
				log_.Print((*it_file)->file_name_.c_str());
				log_.print_line(ckT(" (%u:%u)"),(*it_file)->extent_loc_,(*it_file)->extent_len_);
			}
		}
	}

	void Iso9660Reader::print_tree()
	{
		if (root_node_ == NULL)
			return;

		Iso9660TreeNode *cur_node = root_node_;
		int indent = 0;

		log_.print_line(ckT("Iso9660Reader::print_tree"));
#ifdef _WINDOWS
		log_.print_line(ckT("  <root> (%I64u:%I64u)"),cur_node->extent_loc_,cur_node->extent_len_);
#else
		log_.print_line(ckT("  <root> (%llu:%llu)"),cur_node->extent_loc_,cur_node->extent_len_);
#endif

		std::vector<std::pair<Iso9660TreeNode *,int> > dir_node_stack;
		print_local_tree(dir_node_stack,cur_node,4);

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack[dir_node_stack.size() - 1].first;
			indent = dir_node_stack[dir_node_stack.size() - 1].second;

			dir_node_stack.pop_back();

			// Print the directory name.
			for (int i = 0; i < indent; i++)
				log_.Print(ckT(" "));

			log_.Print(ckT("<d>"));
			log_.Print(cur_node->file_name_.c_str());
#ifdef _WINDOWS
			log_.print_line(ckT(" (%I64u:%I64u)"),cur_node->extent_loc_,cur_node->extent_len_);
#else
			log_.print_line(ckT(" (%llu:%llu)"),cur_node->extent_loc_,cur_node->extent_len_);
#endif

			print_local_tree(dir_node_stack,cur_node,indent + 2);
		}
	}
#endif
};
