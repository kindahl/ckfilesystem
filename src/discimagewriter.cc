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

#include <ckcore/string.hh>
#include "ckfilesystem/stringtable.hh"
#include "ckfilesystem/sectormanager.hh"
#include "ckfilesystem/iso9660writer.hh"
#include "ckfilesystem/udfwriter.hh"
#include "ckfilesystem/dvdvideo.hh"
#include "ckfilesystem/discimagewriter.hh"

namespace ckfilesystem
{
	DiscImageWriter::DiscImageWriter(ckcore::Log &log,FileSystem file_sys) :
		file_sys_(file_sys),log_(log),eltorito_(log),udf_(file_sys == FS_DVDVIDEO)
	{
	}

	DiscImageWriter::~DiscImageWriter()
	{
	}

	/*
		Calculates file system specific data such as extent location and size for a
		single file.
	 */
	bool DiscImageWriter::calc_local_filesys_data(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
			                                      FileTreeNode *local_node,int level,
											      ckcore::tuint64 &sec_offset,ckcore::Progress &progress)
	{
		std::vector<FileTreeNode *>::const_iterator it_file;
		for (it_file = local_node->children_.begin(); it_file !=
			local_node->children_.end(); it_file++)
		{
			if ((*it_file)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (level >= iso9660_.get_max_dir_level())
					continue;
				else
					dir_node_stack.push_back(std::make_pair(*it_file,level + 1));
			}
			else
			{
				// Validate file size.
				if ((*it_file)->file_size_ > ISO9660_MAX_EXTENT_SIZE && !iso9660_.allows_fragmentation())
				{
					if (file_sys_ == FS_ISO9660 || file_sys_ == FS_ISO9660_JOLIET || file_sys_ == FS_DVDVIDEO)
					{
						log_.print_line(ckT("  Warning: Skipping \"%s\", the file is larger than 4 GiB."),
							(*it_file)->file_name_.c_str());
						progress.notify(ckcore::Progress::ckWARNING,StringTable::instance().get_string(StringTable::WARNING_SKIP4GFILE),
							(*it_file)->file_name_.c_str());

						continue;
					}
					else if (file_sys_ == FS_ISO9660_UDF || file_sys_ == FS_ISO9660_UDF_JOLIET)
					{
						log_.print_line(ckT("  Warning: The file \"%s\" is larger than 4 GiB. It will not be visible in the ISO9660/Joliet file system."),
							(*it_file)->file_name_.c_str());
						progress.notify(ckcore::Progress::ckWARNING,StringTable::instance().get_string(StringTable::WARNING_SKIP4GFILEISO),
							(*it_file)->file_name_.c_str());
					}
				}

				// If imported, use the imported information.
				if ((*it_file)->file_flags_ & FileTreeNode::FLAG_IMPORTED)
				{
					Iso9660ImportData *import_node_ptr = (Iso9660ImportData *)(*it_file)->data_ptr_;
					if (import_node_ptr == NULL)
					{
						log_.print_line(ckT("  Error: The file \"%s\" does not contain imported session data like advertised."),
							(*it_file)->file_name_.c_str());
						return false;
					}

					(*it_file)->data_size_normal_ = import_node_ptr->extent_len_;
					(*it_file)->data_size_joliet_ = import_node_ptr->extent_len_;

					(*it_file)->data_pos_normal_ = import_node_ptr->extent_loc_;
					(*it_file)->data_pos_joliet_ = import_node_ptr->extent_loc_;
				}
				else
				{
					(*it_file)->data_size_normal_ = (*it_file)->file_size_;
					(*it_file)->data_size_joliet_ = (*it_file)->file_size_;

					(*it_file)->data_pos_normal_ = sec_offset;
					(*it_file)->data_pos_joliet_ = sec_offset;

					sec_offset += (*it_file)->data_size_normal_/ISO9660_SECTOR_SIZE;
					if ((*it_file)->data_size_normal_ % ISO9660_SECTOR_SIZE != 0)
						sec_offset++;

					// Pad if necessary.
					sec_offset += (*it_file)->data_pad_len_;
				}
			}
		}

		return true;
	}

	/*
		Calculates file system specific data such as location of extents and sizes of
		extents.
	*/
	bool DiscImageWriter::calc_filesys_data(FileTree &file_tree,ckcore::Progress &progress,
						    			    ckcore::tuint64 start_sec,
										    ckcore::tuint64 &last_sec)
	{
		FileTreeNode *cur_node = file_tree.get_root();
		ckcore::tuint64 sec_offset = start_sec;

		std::vector<std::pair<FileTreeNode *,int> > dir_node_stack;
		if (!calc_local_filesys_data(dir_node_stack,cur_node,0,sec_offset,progress))
			return false;

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack[dir_node_stack.size() - 1].first;
			int level = dir_node_stack[dir_node_stack.size() - 1].second;
			dir_node_stack.pop_back();

			if (!calc_local_filesys_data(dir_node_stack,cur_node,level,sec_offset,progress))
				return false;
		}

		last_sec = sec_offset;
		return true;
	}

	int DiscImageWriter::write_file_node(SectorOutStream &out_stream,FileTreeNode *node,
									     ckcore::Progresser &progresser)
	{
		ckcore::FileInStream fs(node->file_path_.c_str());
		if (!fs.open())
		{
			log_.print_line(ckT("  Error: Unable to obtain file handle to \"%s\"."),
				node->file_path_.c_str());
			progresser.notify(ckcore::Progress::ckERROR,
				StringTable::instance().get_string(StringTable::ERROR_OPENREAD),
				node->file_path_.c_str());
			return RESULT_FAIL;
		}

		if (!ckcore::stream::copy(fs,out_stream,progresser))
		{
			log_.print_line(ckT("  Error: Unable write file to disc image."));
			return RESULT_FAIL;
		}

		// Pad the sector.
		if (out_stream.get_allocated() != 0)
			out_stream.pad_sector();

		return RESULT_OK;
	}

	int DiscImageWriter::write_local_file_data(SectorOutStream &out_stream,
										       std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
											   FileTreeNode *local_node,int level,
											   ckcore::Progresser &progresser)
	{
		std::vector<FileTreeNode *>::const_iterator it_file;
		for (it_file = local_node->children_.begin(); it_file !=
			local_node->children_.end(); it_file++)
		{
			// Check if we should abort.
			if (progresser.cancelled())
				return RESULT_CANCEL;

			if ((*it_file)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				// Validate directory level.
				if (level >= iso9660_.get_max_dir_level())
					continue;
				else
					dir_node_stack.push_back(std::make_pair(*it_file,level + 1));
			}
			else if (!((*it_file)->file_flags_ & FileTreeNode::FLAG_IMPORTED))	// We don't have any data to write for imported files.
			{
				// Validate file size.
				if (file_sys_ == FS_ISO9660 || file_sys_ == FS_ISO9660_JOLIET || file_sys_ == FS_DVDVIDEO)
				{
					if ((*it_file)->file_size_ > ISO9660_MAX_EXTENT_SIZE && !iso9660_.allows_fragmentation())
						continue;
				}

				switch (write_file_node(out_stream,*it_file,progresser))
				{
					case RESULT_FAIL:
#ifdef _WINDOWS
						log_.print_line(ckT("  Error: Unable to write node \"%s\" to (%I64u,%I64u)."),
#else
						log_.print_line(ckT("  Error: Unable to write node \"%s\" to (%llu,%llu)."),
#endif
							(*it_file)->file_name_.c_str(),(*it_file)->data_pos_normal_,(*it_file)->data_size_normal_);
						return RESULT_FAIL;

					case RESULT_CANCEL:
						return RESULT_CANCEL;
				}

				// Pad if necessary.
				char tmp[1] = { 0 };
				for (unsigned int i = 0; i < (*it_file)->data_pad_len_; i++)
				{
					for (unsigned int j = 0; j < ISO9660_SECTOR_SIZE; j++)
						out_stream.write(tmp,1);
				}
			}
		}

		return RESULT_OK;
	}

	int DiscImageWriter::write_file_data(SectorOutStream &out_stream,FileTree &file_tree,
									     ckcore::Progresser &progresser)
	{
		FileTreeNode *cur_node = file_tree.get_root();

		std::vector<std::pair<FileTreeNode *,int> > dir_node_stack;
		int res = write_local_file_data(out_stream,dir_node_stack,cur_node,1,progresser);
		if (res != RESULT_OK)
			return res;

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack[dir_node_stack.size() - 1].first;
			int level = dir_node_stack[dir_node_stack.size() - 1].second;
			dir_node_stack.pop_back();

			res = write_local_file_data(out_stream,dir_node_stack,cur_node,level,progresser);
			if (res != RESULT_OK)
				return res;
		}

		return RESULT_OK;
	}

	void DiscImageWriter::get_internal_path(FileTreeNode *child_node,ckcore::tstring &node_path,
										    bool ext_path,bool joliet)
	{
		node_path = ckT("/");

		if (ext_path)
		{
			// Joliet or ISO9660?
			if (joliet)
			{
#ifdef _UNICODE
				if (child_node->file_name_joliet_[child_node->file_name_joliet_.length() - 2] == ';')
					node_path.append(child_node->file_name_joliet_,0,child_node->file_name_joliet_.length() - 2);
				else
					node_path.append(child_node->file_name_joliet_);
#else
				char szAnsiName[JOLIET_MAX_NAMELEN_RELAXED + 1];
				ckcore::string::utf16_to_ansi(child_node->file_name_joliet_.c_str(),szAnsiName,sizeof(szAnsiName));

				if (szAnsiName[child_node->file_name_joliet_.length() - 2] == ';')
					szAnsiName[child_node->file_name_joliet_.length() - 2] = '\0';

				node_path.append(szAnsiName);
#endif
			}
			else
			{
#ifdef _UNICODE
				wchar_t utf_file_name[MAX_PATH];
				ckcore::string::ansi_to_utf16(child_node->file_name_iso9660_.c_str(),utf_file_name,
											  sizeof(utf_file_name)/sizeof(wchar_t));

				if (utf_file_name[child_node->file_name_iso9660_.length() - 2] == ';')
					utf_file_name[child_node->file_name_iso9660_.length() - 2] = '\0';

				node_path.append(utf_file_name);
#else
				if (child_node->file_name_iso9660_[child_node->file_name_iso9660_.length() - 2] == ';')
					node_path.append(child_node->file_name_iso9660_,0,child_node->file_name_iso9660_.length() - 2);
				else
					node_path.append(child_node->file_name_iso9660_);
#endif
			}
		}
		else
		{
			node_path.append(child_node->file_name_);
		}

		FileTreeNode *cur_node = child_node->get_parent();
		while (cur_node->get_parent() != NULL)
		{
			if (ext_path)
			{
				// Joliet or ISO9660?
				if (joliet)
				{
	#ifdef _UNICODE
					if (cur_node->file_name_joliet_[cur_node->file_name_joliet_.length() - 2] == ';')
					{
						std::wstring::iterator itEnd = cur_node->file_name_joliet_.end();
						itEnd--;
						itEnd--;

						node_path.insert(node_path.begin(),cur_node->file_name_joliet_.begin(),itEnd);
					}
					else
					{
						node_path.insert(node_path.begin(),cur_node->file_name_joliet_.begin(),
							cur_node->file_name_joliet_.end());
					}
	#else
					char szAnsiName[JOLIET_MAX_NAMELEN_RELAXED + 1];
					ckcore::string::utf16_to_ansi(cur_node->file_name_joliet_.c_str(),szAnsiName,sizeof(szAnsiName));

					if (szAnsiName[cur_node->file_name_joliet_.length() - 2] == ';')
						szAnsiName[cur_node->file_name_joliet_.length() - 2] = '\0';

					node_path.insert(0,szAnsiName);
	#endif
				}
				else
				{
	#ifdef _UNICODE
					wchar_t utf_file_name[MAX_PATH];
					ckcore::string::ansi_to_utf16(cur_node->file_name_iso9660_.c_str(),utf_file_name,
												  sizeof(utf_file_name)/sizeof(wchar_t));
					node_path.insert(0,utf_file_name);

					if (utf_file_name[cur_node->file_name_iso9660_.length() - 2] == ';')
						utf_file_name[cur_node->file_name_iso9660_.length() - 2] = '\0';
	#else
					if (cur_node->file_name_iso9660_[cur_node->file_name_iso9660_.length() - 2] == ';')
					{
						std::string::iterator itEnd = cur_node->file_name_iso9660_.end();
						itEnd--;
						itEnd--;

						node_path.insert(node_path.begin(),cur_node->file_name_iso9660_.begin(),itEnd);
					}
					else
					{
						node_path.insert(node_path.begin(),cur_node->file_name_iso9660_.begin(),
							cur_node->file_name_iso9660_.end());
					}
	#endif
				}
			}
			else
			{
				node_path.insert(node_path.begin(),cur_node->file_name_.begin(),
					cur_node->file_name_.end());
			}

			node_path.insert(0,ckT("/"));

			cur_node = cur_node->get_parent();
		}
	}

	void DiscImageWriter::create_local_file_path_map(FileTreeNode *local_node,
												 std::vector<FileTreeNode *> &dir_node_stack,
												 std::map<ckcore::tstring,ckcore::tstring> &filepath_map,
												 bool joliet)
	{
		std::vector<FileTreeNode *>::const_iterator it_file;
		for (it_file = local_node->children_.begin(); it_file !=
			local_node->children_.end(); it_file++)
		{
			if ((*it_file)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				dir_node_stack.push_back(*it_file);
			}
			else
			{
				// Yeah, this is not very efficient. Both paths should be calculated togather.
				ckcore::tstring file_path;
				get_internal_path(*it_file,file_path,false,joliet);

				ckcore::tstring ExternalFilePath;
				get_internal_path(*it_file,ExternalFilePath,true,joliet);

				filepath_map[file_path] = ExternalFilePath;
			}
		}
	}

	/*
		Used for creating a map between the internal file names and the
		external (Joliet or ISO9660, in that order).
	*/
	void DiscImageWriter::create_file_path_map(FileTree &file_tree,
											std::map<ckcore::tstring,ckcore::tstring> &filepath_map,
											bool joliet)
	{
		FileTreeNode *cur_node = file_tree.get_root();

		std::vector<FileTreeNode *> dir_node_stack;
		create_local_file_path_map(cur_node,dir_node_stack,filepath_map,joliet);

		while (dir_node_stack.size() > 0)
		{ 
			cur_node = dir_node_stack.back();
			dir_node_stack.pop_back();

			create_local_file_path_map(cur_node,dir_node_stack,filepath_map,joliet);
		}
	}

	/*
		sec_offset is a space assumed to be allocated before this image,
		this is used for creating multi-session discs.
		pFileNameMap is optional, it should be specified if one wants to map the
		internal file paths to the actual external paths.
	 */
	int DiscImageWriter::create(SectorOutStream &out_stream,FileSet &files,
								ckcore::Progress &progress,unsigned long sec_offset,
								std::map<ckcore::tstring,ckcore::tstring> *filepath_map_ptr)
	{
		log_.print_line(ckT("DiscImageWriter::Create"));
		log_.print_line(ckT("  Sector offset: %u."),sec_offset);

		// The first 16 sectors are reserved for system use (write 0s).
		char tmp[1] = { 0 };
		for (unsigned int i = 0; i < ISO9660_SECTOR_SIZE << 4; i++)
			out_stream.write(tmp,1);

		progress.set_status(StringTable::instance().get_string(StringTable::STATUS_BUILDTREE));
		progress.set_marquee(true);

		// Create a file tree.
		FileTree file_tree(log_);
		if (!file_tree.create_from_file_set(files))
		{
			log_.print_line(ckT("  Error: failed to build file tree."));
			return fail(RESULT_FAIL,out_stream);
		}

		// Calculate padding if DVD-Video file system.
		if (file_sys_ == FS_DVDVIDEO)
		{
			DvdVideo dvd_video(log_);
			if (!dvd_video.calc_file_padding(file_tree))
			{
				log_.print_line(ckT("  Error: failed to calculate file padding for DVD-Video file system."));
				return fail(RESULT_FAIL,out_stream);
			}

			dvd_video.print_file_padding(file_tree);
		}

		bool use_iso = file_sys_ != FS_UDF;
		bool use_udf = file_sys_ == FS_ISO9660_UDF || file_sys_ == FS_ISO9660_UDF_JOLIET ||
			file_sys_ == FS_UDF || file_sys_ == FS_DVDVIDEO;
		bool use_joliet = file_sys_ == FS_ISO9660_JOLIET || file_sys_ == FS_ISO9660_UDF_JOLIET;

		SectorManager sec_manager(16 + sec_offset);
		Iso9660Writer iso_writer(log_,out_stream,sec_manager,iso9660_,joliet_,eltorito_,true,use_joliet);
		UdfWriter udf_writer(log_,out_stream,sec_manager,udf_,true);

		int res = RESULT_FAIL;

		// FIXME: Put failure messages to Progress.
		if (use_iso)
		{
			res = iso_writer.alloc_header();
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		if (use_udf)
		{
			res = udf_writer.alloc_header();
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		if (use_iso)
		{
			res = iso_writer.alloc_path_tables(progress,files);
			if (res != RESULT_OK)
				return fail(res,out_stream);

			res = iso_writer.alloc_dir_entries(file_tree);
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		if (use_udf)
		{
			res = udf_writer.alloc_partition(file_tree);
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		// Allocate file data.
		ckcore::tuint64 first_data_sec = sec_manager.get_next_free();
		ckcore::tuint64 last_data_sec = 0;

		if (!calc_filesys_data(file_tree,progress,first_data_sec,last_data_sec))
		{
			log_.print_line(ckT("  Error: Could not calculate necessary file system information."));
			return fail(RESULT_FAIL,out_stream);
		}

		sec_manager.alloc_data_sectors(last_data_sec - first_data_sec);

		if (use_iso)
		{
			res = iso_writer.write_header(files,file_tree);
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		if (use_udf)
		{
			res = udf_writer.write_header();
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		// FIXME: Add progress for this.
		if (use_iso)
		{
			res = iso_writer.write_path_tables(files,file_tree,progress);
			if (res != RESULT_OK)
				return fail(res,out_stream);

			res = iso_writer.write_dir_entries(file_tree,progress);
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		if (use_udf)
		{
			res = udf_writer.write_partition(file_tree);
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

		progress.set_status(StringTable::instance().get_string(StringTable::STATUS_WRITEDATA));
		progress.set_marquee(false);

		// To help keep track of the progress.
		ckcore::Progresser progresser(progress,sec_manager.get_data_length() * ISO9660_SECTOR_SIZE);
		res = write_file_data(out_stream,file_tree,progresser);
		if (res != RESULT_OK)
			return fail(res,out_stream);

		if (use_udf)
		{
			res = udf_writer.write_tail();
			if (res != RESULT_OK)
				return fail(res,out_stream);
		}

#ifdef _DEBUG
		file_tree.PrintTree();
#endif

		// Map the paths if requested.
		if (filepath_map_ptr != NULL)
			create_file_path_map(file_tree,*filepath_map_ptr,use_joliet);

		out_stream.flush();
		return RESULT_OK;
	}

	/*
		Should be called when create operation fails or cancel so that the
		broken image can be removed and the file handle closed.
	 */
	int DiscImageWriter::fail(int res,SectorOutStream &out_stream)
	{
		out_stream.flush();
		return res;
	}

	void DiscImageWriter::set_volume_label(const ckcore::tchar *label)
	{
		iso9660_.set_volume_label(label);
		joliet_.set_volume_label(label);
		udf_.set_volume_label(label);
	}

	void DiscImageWriter::set_text_fields(const ckcore::tchar *sys_ident,
										  const ckcore::tchar *volset_ident,
										  const ckcore::tchar *publ_ident,
										  const ckcore::tchar *prep_ident)
	{
		iso9660_.set_text_fields(sys_ident,volset_ident,publ_ident,prep_ident);
		joliet_.set_text_fields(sys_ident,volset_ident,publ_ident,prep_ident);
	}

	void DiscImageWriter::set_file_fields(const ckcore::tchar *copy_file_ident,
										  const ckcore::tchar *abst_file_ident,
										  const ckcore::tchar *bibl_file_ident)
	{
		iso9660_.set_file_fields(copy_file_ident,abst_file_ident,bibl_file_ident);
		joliet_.set_file_fields(copy_file_ident,abst_file_ident,bibl_file_ident);
	}

	void DiscImageWriter::set_interchange_level(Iso9660::InterLevel inter_level)
	{
		iso9660_.set_interchange_level(inter_level);
	}

	void DiscImageWriter::set_include_file_ver_info(bool include)
	{
		iso9660_.set_include_file_ver_info(include);
		joliet_.set_include_file_ver_info(include);
	}

	void DiscImageWriter::set_part_access_type(Udf::PartAccessType access_type)
	{
		udf_.set_part_access_type(access_type);
	}

	void DiscImageWriter::set_relax_max_dir_level(bool relax)
	{
		iso9660_.set_relax_max_dir_level(relax);
	}

	void DiscImageWriter::set_long_joliet_names(bool enable)
	{
		joliet_.set_relax_max_name_len(enable);
	}

	bool DiscImageWriter::add_boot_image_no_emu(const ckcore::tchar *full_path,bool bootable,
											    unsigned short load_segment,unsigned short sec_count)
	{
		return eltorito_.add_boot_image_no_emu(full_path,bootable,load_segment,sec_count);
	}

	bool DiscImageWriter::add_boot_image_floppy(const ckcore::tchar *full_path,bool bootable)
	{
		return eltorito_.add_boot_image_floppy(full_path,bootable);
	}

	bool DiscImageWriter::add_boot_image_hard_disk(const ckcore::tchar *full_path,bool bootable)
	{
		return eltorito_.add_boot_image_hard_disk(full_path,bootable);
	}
};
