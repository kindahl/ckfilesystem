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
#include <ckcore/string.hh>
#include "ckfilesystem/filetree.hh"
#include "ckfilesystem/filesystem.hh"

namespace ckfilesystem
{
    typedef std::pair<FileTreeNode *,ckcore::tuint16> Iso9660PathTableEntry;
    typedef std::vector<Iso9660PathTableEntry> Iso9660PathTable;

    namespace iso9660pathtable
    {
        int level(const FileTreeNode *node);

        void populate_from_tree(Iso9660PathTable &pt,FileTree &tree,
                                FileSystem &file_sys,ckcore::Progress &progress);
        void sort(Iso9660PathTable &pt,bool joliet,bool dvdvideo);
    };
};
