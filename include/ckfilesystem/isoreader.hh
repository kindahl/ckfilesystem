/*
 * The ckFileSystem library provides file system functionality.
 * Copyright (C) 2006-2011 Christian Kindahl
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
#include <vector>
#include <ckcore/types.hh>
#include <ckcore/log.hh>
#include <ckcore/stream.hh>
#include "ckfilesystem/iso.hh"

namespace ckfilesystem
{
    // A Iso9660TreeNode tree node contains every information needed to write
    // an ISO9660 directory record.
    class Iso9660TreeNode
    {
    private:
        Iso9660TreeNode *parent_node_;

    public:
        std::vector<Iso9660TreeNode *> children_;

        unsigned char file_flags_;
        unsigned char file_unit_size_;
        unsigned char interleave_gap_size_;
        ckcore::tuint16 volseq_num_;
        ckcore::tuint32 extent_loc_;
        ckcore::tuint32 extent_len_;

        tiso_dir_record_datetime rec_timestamp_;

        ckcore::tstring file_name_;

        Iso9660TreeNode(Iso9660TreeNode *parent_node,const ckcore::tchar *file_name,
                        ckcore::tuint32 extent_loc,ckcore::tuint32 extent_len,
                        ckcore::tuint16 volseq_num,unsigned char file_flags,
                        unsigned char file_unit_size,unsigned char interleave_gap_size,
                        tiso_dir_record_datetime &rec_timestamp) :
            parent_node_(parent_node),file_flags_(file_flags),
            file_unit_size_(file_unit_size),interleave_gap_size_(interleave_gap_size),
            volseq_num_(volseq_num),extent_loc_(extent_loc),extent_len_(extent_len)
        {
            memcpy(&rec_timestamp_,&rec_timestamp,sizeof(tiso_dir_record_datetime));

            if (file_name != NULL)
                file_name_ = file_name;
        }

        ~Iso9660TreeNode()
        {
            // Free the children.
            std::vector<Iso9660TreeNode *>::iterator it_node;
            for (it_node = children_.begin(); it_node != children_.end(); it_node++)
                delete *it_node;

            children_.clear();
        }

        Iso9660TreeNode *get_parent()
        {
            return parent_node_;
        }
    };

    class Iso9660Reader
    {
    private:
        ckcore::Log &log_;

        Iso9660TreeNode *root_node_;

        bool read_dir_entry(ckcore::InStream &in_stream,
                            std::vector<Iso9660TreeNode *> &dir_entries,
                            Iso9660TreeNode *parent_node,
                            bool joliet);

    public:
        Iso9660Reader(ckcore::Log &log);
        ~Iso9660Reader();

        Iso9660TreeNode *get_root()
        {
            return root_node_;
        }

        bool read(ckcore::InStream &in_stream,ckcore::tuint32 start_sec);

    #ifdef _DEBUG
        void print_local_tree(std::vector<std::pair<Iso9660TreeNode *,int> > &dir_node_stack,
                              Iso9660TreeNode *local_node,int indent);
        void print_tree();
    #endif
    };
};
