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

#include "ckfilesystem/filetree.hh"

namespace ckfilesystem
{
	FileTree::FileTree(ckcore::Log &log) :
		log_(log),root_node_(NULL),dir_count_(0),file_count_(0)
	{
	}

	FileTree::~FileTree()
	{
		if (root_node_ != NULL)
		{
			delete root_node_;
			root_node_ = NULL;
		}
	}

	FileTreeNode *FileTree::GetRoot()
	{
		return root_node_;
	}

	FileTreeNode *FileTree::GetChildFromFileName(FileTreeNode *parent_node,const ckcore::tchar *file_name)
	{
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = parent_node->children_.begin(); it !=
			parent_node->children_.end(); it++)
		{
			if (!ckcore::string::astrcmp(file_name,(*it)->file_name_.c_str()))
				return *it;
		}

		return NULL;
	}

	bool FileTree::AddFileFromPath(const FileDescriptor &file)
	{
		size_t dir_path_len = file.internal_path_.length(),prev_delim = 0,pos;
		ckcore::tstring cur_dir_name;
		FileTreeNode *cur_node = root_node_;

		for (pos = 0; pos < dir_path_len; pos++)
		{
			if (file.internal_path_.c_str()[pos] == '/')
			{
				if (pos > (prev_delim + 1))
				{
					// Obtain the name of the current directory.
					cur_dir_name.erase();
					for (size_t j = prev_delim + 1; j < pos; j++)
						cur_dir_name.push_back(file.internal_path_.c_str()[j]);

					cur_node = GetChildFromFileName(cur_node,cur_dir_name.c_str());
					if (cur_node == NULL)
					{
						log_.PrintLine(ckT("  Error: Unable to find child node \"%s\" in path \"%s\"."),
							cur_dir_name.c_str(),file.internal_path_.c_str());
						return false;
					}
				}

				prev_delim = pos;
			}
		}

		// We now have our parent.
		const ckcore::tchar *file_name = file.internal_path_.c_str() + prev_delim + 1;

		// Check if imported.
		unsigned char import_flac = 0;
		void *import_data_ptr = NULL;
		if (file.flags_ & FileDescriptor::FLAG_IMPORTED)
		{
			import_flac = FileTreeNode::FLAG_IMPORTED;
			import_data_ptr = file.data_ptr_;
		}

		if (file.flags_ & FileDescriptor::FLAG_DIRECTORY)
		{
			cur_node->children_.push_back(new FileTreeNode(cur_node,file_name,
				file.external_path_.c_str(),0,true,0,FileTreeNode::FLAG_DIRECTORY | import_flac,
				import_data_ptr));

			dir_count_++;
		}
		else
		{
			cur_node->children_.push_back(new FileTreeNode(cur_node,file_name,
				file.external_path_.c_str(),file.file_size_,true,0,import_flac,import_data_ptr));

			file_count_++;
		}

		return true;
	}

	bool FileTree::CreateFromFileSet(const FileSet &files)
	{
		if (root_node_ != NULL)
			delete root_node_;

		root_node_ = new FileTreeNode(NULL,ckT(""),ckT(""),0,true,0,
			FileTreeNode::FLAG_DIRECTORY);

		FileSet::const_iterator it;
		for (it = files.begin(); it != files.end(); it++)
		{
			if (!AddFileFromPath(*it))
				return false;
		}

		return true;
	}

	FileTreeNode *FileTree::GetNodeFromPath(const FileDescriptor &file)
	{
		//m_pLog->PrintLine(ckT("BEGIN: %s"),file.m_ExternalPath.c_str());
		size_t dir_path_len = file.internal_path_.length(),prev_delim = 0,pos;
		ckcore::tstring cur_dir_name;
		FileTreeNode *cur_node = root_node_;

		for (pos = 0; pos < dir_path_len; pos++)
		{
			if (file.internal_path_.c_str()[pos] == '/')
			{
				if (pos > (prev_delim + 1))
				{
					// Obtain the name of the current directory.
					cur_dir_name.erase();
					for (size_t j = prev_delim + 1; j < pos; j++)
						cur_dir_name.push_back(file.internal_path_.c_str()[j]);

					cur_node = GetChildFromFileName(cur_node,cur_dir_name.c_str());
					if (cur_node == NULL)
					{
						log_.PrintLine(ckT("  Error: Unable to find child node \"%s\"."),cur_dir_name.c_str());
						return NULL;
					}
				}

				prev_delim = pos;
			}
		}

		// We now have our parent.
		const ckcore::tchar *file_name = file.internal_path_.c_str() + prev_delim + 1;

		//m_pLog->PrintLine(ckT("  END: %s"),file.m_ExternalPath.c_str());
		return GetChildFromFileName(cur_node,file_name);
	}

	FileTreeNode *FileTree::GetNodeFromPath(const ckcore::tchar *internal_path)
	{
		size_t dir_path_len = ckcore::string::astrlen(internal_path),prev_delim = 0,pos;
		ckcore::tstring cur_dir_name;
		FileTreeNode *cur_node = root_node_;

		for (pos = 0; pos < dir_path_len; pos++)
		{
			if (internal_path[pos] == '/')
			{
				if (pos > (prev_delim + 1))
				{
					// Obtain the name of the current directory.
					cur_dir_name.erase();
					for (size_t j = prev_delim + 1; j < pos; j++)
						cur_dir_name.push_back(internal_path[j]);

					cur_node = GetChildFromFileName(cur_node,cur_dir_name.c_str());
					if (cur_node == NULL)
					{
						log_.PrintLine(ckT("  Error: Unable to find child node \"%s\"."),cur_dir_name.c_str());
						return NULL;
					}
				}

				prev_delim = pos;
			}
		}

		// We now have our parent.
		const ckcore::tchar *file_name = internal_path + prev_delim + 1;

		return GetChildFromFileName(cur_node,file_name);
	}

	/**
		@eturn the number of files in the tree, fragmented files are counted once.
	*/
	unsigned long FileTree::GetDirCount()
	{
		return dir_count_;
	}

	/**
		@return the number of directories in the tree, the root is not included.
	*/
	unsigned long FileTree::GetFileCount()
	{
		return file_count_;
	}

#ifdef _DEBUG
	void FileTree::PrintLocalTree(std::vector<std::pair<FileTreeNode *,int> > &dir_node_stack,
								   FileTreeNode *local_node,int indent)
	{
		std::vector<FileTreeNode *>::const_iterator it;
		for (it = local_node->children_.begin(); it !=
			local_node->children_.end(); it++)
		{
			if ((*it)->file_flags_ & FileTreeNode::FLAG_DIRECTORY)
			{
				dir_node_stack.push_back(std::make_pair(*it,indent));
			}
			else
			{
				for (int i = 0; i < indent; i++)
					log_.Print(ckT(" "));

				log_.Print(ckT("<f>"));
				log_.Print((*it)->file_name_.c_str());
#ifdef _WINDOWS
				log_.PrintLine(ckT(" (%I64u:%I64u,%I64u:%I64u,%I64u:%I64u)"),(*it)->data_pos_normal_,
#else
				log_.PrintLine(ckT(" (%llu:%llu,%llu:%llu,%llu:%llu)"),(*it)->data_pos_normal_,
#endif
					(*it)->data_size_normal_,(*it)->data_pos_joliet_,
					(*it)->data_size_joliet_,(*it)->udf_size_,(*it)->udf_size_tot_);
			}
		}
	}

	void FileTree::PrintTree()
	{
		if (root_node_ == NULL)
			return;

		FileTreeNode *cur_node = root_node_;
		int indent = 0;

		log_.PrintLine(ckT("FileTree::PrintTree"));
#ifdef _WINDOWS
		log_.PrintLine(ckT("  <root> (%I64u:%I64u,%I64u:%I64u,%I64u:%I64u)"),cur_node->data_pos_normal_,
#else
		log_.PrintLine(ckT("  <root> (%llu:%llu,%llu:%llu,%llu:%llu)"),cur_node->data_pos_normal_,
#endif
		cur_node->data_size_normal_,cur_node->data_pos_joliet_,
		cur_node->data_size_joliet_,cur_node->udf_size_,cur_node->udf_size_tot_);

		std::vector<std::pair<FileTreeNode *,int> > dir_node_stack;
		PrintLocalTree(dir_node_stack,cur_node,4);

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
			log_.PrintLine(ckT(" (%I64u:%I64u,%I64u:%I64u,%I64u:%I64u)"),cur_node->data_pos_normal_,
#else
			log_.PrintLine(ckT(" (%llu:%llu,%llu:%llu,%llu:%llu)"),cur_node->data_pos_normal_,
#endif
				cur_node->data_size_normal_,cur_node->data_pos_joliet_,
				cur_node->data_size_joliet_,cur_node->udf_size_,cur_node->udf_size_tot_);

			PrintLocalTree(dir_node_stack,cur_node,indent + 2);
		}
	}
#endif
};
