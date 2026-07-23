/*
Copyright (©) 2026  Frosty515

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

#include "Memory.hpp"
#include "SystemCall.hpp"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <util.h>

#include <fs/FDManager.hpp>
#include <fs/FileDescriptor.hpp>
#include <fs/VFS.hpp>

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>
#include <Scheduling/Thread.hpp>

bool ValidateMmapFlags(int flags) {
    if (flags <= 0 || flags >= (MAP_FIXED_NOREPLACE * 2)) // out of bounds
        return false;
    if ((flags & (MAP_SHARED | MAP_PRIVATE)) == 0 || (flags & (MAP_SHARED | MAP_PRIVATE)) == (MAP_SHARED | MAP_PRIVATE)) // neither shared or private OR both shared and private
        return false;
    if ((flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) == (MAP_FIXED | MAP_FIXED_NOREPLACE)) // can't be both FIXED and FIXED_NOREPLACE
        return false;
    return true;
}

void* sys_mmap(void* addr, size_t length, int prot, int flags, sys_mmapExtraArgs* args) {
    if ((addr == nullptr && (flags & MAP_FIXED) > 0) || length == 0
            || prot > (PROT_READ | PROT_WRITE | PROT_EXEC)
            || !ValidateMmapFlags(flags))
        return (void*)-EINVAL;

    if (prot == (PROT_WRITE | PROT_EXEC))
        return (void*)-ENOSYS;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr)
        return (void*)-ENOSYS;

    sys_mmapExtraArgs kArgs;
    if (!UserRead(args, &kArgs, sizeof(sys_mmapExtraArgs), proc))
        return (void*)-EFAULT;

    if (addr != nullptr)
        addr = ALIGN_DOWN_ADDRESS(addr, PAGE_SIZE);

    uint64_t pageCount = DIV_ROUNDUP(length, PAGE_SIZE);

    VMM::Protection protection = VMM::Protection::NONE;
    if (prot & PROT_READ)
        protection = (VMM::Protection)((uint8_t)protection | (uint8_t)VMM::Protection::READ);
    if (prot & PROT_WRITE)
        protection = (VMM::Protection)((uint8_t)protection | (uint8_t)VMM::Protection::WRITE);
    if (prot & PROT_EXEC)
        protection = (VMM::Protection)((uint8_t)protection | (uint8_t)VMM::Protection::EXECUTE);

    void* mem = nullptr;
    if ((flags & MAP_ANONYMOUS) > 0) {
        VMM::AllocFlags allocFlags = VMM::DEFAULT_ALLOC_FLAGS;
        allocFlags.protection = protection;
        allocFlags.isPrivate = (flags & MAP_PRIVATE) > 0;
        allocFlags.allocPhys = (flags & MAP_POPULATE) > 0;
        allocFlags.addrIsHint = (flags & (MAP_FIXED | MAP_FIXED_NOREPLACE)) == 0;
        allocFlags.replace = (flags & MAP_FIXED) > 0;
        mem = vmm->AllocateAnonPages(pageCount, addr, allocFlags);
        if (mem == nullptr)
            return (void*)((flags & MAP_FIXED) == 0 ? (int64_t)-ENOMEM : (int64_t)-EEXIST);
    } else {
        FileDescriptorManager* manager = proc->GetFDManager();
        if (manager == nullptr)
            return (void*)-ENOSYS;

        FileDescriptor* desc = manager->Get(kArgs.fd);
        if (desc == nullptr || !desc->isOpen() || desc->GetType() != FDType::File)
            return (void*)-EBADF;

        FS::VNode* vnode = desc->GetVNode();
        if (vnode == nullptr)
            return (void*)-EBADF;

        int rc = FS::VFS_MapFile(addr, length, protection, flags, true, vnode, kArgs.offset, &mem, vmm, proc->GetCred());
        if (rc < 0)
            return (void*)(int64_t)rc;
    }

    return mem;
}

int sys_munmap(void* addr, size_t length) {
    if (addr == nullptr || length == 0)
        return -EINVAL;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr)
        return -ENOSYS;

    addr = ALIGN_DOWN_ADDRESS(addr, PAGE_SIZE);
    uint64_t pageCount = DIV_ROUNDUP(length, PAGE_SIZE);

    return vmm->FreePages(addr, pageCount, true) ? ESUCCESS : -EINVAL;
}

int sys_mprotect(void* addr, size_t size, int prot) {
    if (addr == nullptr || size == 0 || prot <= PROT_NONE || prot > (PROT_READ | PROT_WRITE | PROT_EXEC))
        return -EINVAL;

    if (prot == (PROT_WRITE | PROT_EXEC))
        return -ENOSYS;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr)
        return -ENOSYS;

    addr = ALIGN_DOWN_ADDRESS(addr, PAGE_SIZE);
    uint64_t pageCount = DIV_ROUNDUP(size, PAGE_SIZE);

    VMM::Protection protection = VMM::Protection::NONE;
    if (prot & PROT_READ)
        protection = (VMM::Protection)((uint8_t)protection | (uint8_t)VMM::Protection::READ);
    if (prot & PROT_WRITE)
        protection = (VMM::Protection)((uint8_t)protection | (uint8_t)VMM::Protection::WRITE);
    if (prot & PROT_EXEC)
        protection = (VMM::Protection)((uint8_t)protection | (uint8_t)VMM::Protection::EXECUTE);

    return vmm->RemapPages(addr, pageCount, protection, true, VMM::CacheType::DEFAULT, true) ? ESUCCESS : -EINVAL;
}
