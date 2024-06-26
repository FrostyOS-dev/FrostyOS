/*
Copyright (©) 2023-2024  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _TEMP_FILE_SYSTEM_HPP
#define _TEMP_FILE_SYSTEM_HPP

#include "../FileSystem.hpp"

#include "TempFSInode.hpp"

#include <Data-structures/LinkedList.hpp>

namespace TempFS {

    class TempFileSystem : public FileSystem {
    public:
        TempFileSystem(size_t blockSize, FilePrivilegeLevel rootPrivilege);
        ~TempFileSystem();

        int CreateFile(FilePrivilegeLevel current_privilege, const char* parent, const char* name, size_t size = 0, bool inherit_permissions = true, FilePrivilegeLevel privilege = {0, 0, 00644}) override;
        int CreateFile(FilePrivilegeLevel current_privilege, TempFSInode* parent, const char* name, size_t size = 0, bool inherit_permissions = true, FilePrivilegeLevel privilege = {0, 0, 00644});
        int CreateFolder(FilePrivilegeLevel current_privilege, const char* parent, const char* name, bool inherit_permissions = true, FilePrivilegeLevel privilege = {0, 0, 00644}) override;
        int CreateFolder(FilePrivilegeLevel current_privilege, TempFSInode* parent, const char* name, bool inherit_permissions = true, FilePrivilegeLevel privilege = {0, 0, 00644});
        int CreateSymLink(FilePrivilegeLevel current_privilege, const char* parent, const char* name, const char* target, bool inherit_permissions = true, FilePrivilegeLevel privilege = {0, 0, 00644}) override;
        int CreateSymLink(FilePrivilegeLevel current_privilege, TempFSInode* parent, const char* name, TempFSInode* target, bool inherit_permissions = true, FilePrivilegeLevel privilege = {0, 0, 00644});

        int DeleteInode(FilePrivilegeLevel current_privilege, const char* path, bool recursive = false, bool delete_name = false) override;
        int DeleteInode(FilePrivilegeLevel current_privilege, TempFSInode* inode, bool recursive = false, bool delete_name = false);

        int DestroyFileSystem() override;

        void CreateNewRootInode(TempFSInode* inode);
        void DeleteRootInode(TempFSInode* inode);

        Inode* GetRootInode(uint64_t index, int* status = nullptr) const override; // status will be set if not nullptr
        uint64_t GetRootInodeCount() const override;

        TempFSInode* GetSubInode(TempFSInode* parent, const char* path, TempFSInode** lastInode = nullptr, int64_t* end_index = nullptr, int* status = nullptr);  // last_inode and end_index are only filled if they are non-null pointers. status will be set if not nullptr
        TempFSInode* GetInode(const char* path, TempFSInode** lastInode = nullptr, int64_t* end_index = nullptr, int* status = nullptr); // last_inode and end_index are only filled if they are non-null pointers. status will be set if not nullptr

        FileSystemType GetType() const override;

        FilePrivilegeLevel GetRootPrivilege() const override;

    private:
        FilePrivilegeLevel m_rootPrivilege;
        LinkedList::LockableLinkedList<TempFSInode> m_rootInodes;
    };
}

#endif /* _TEMP_FILE_SYSTEM_HPP */