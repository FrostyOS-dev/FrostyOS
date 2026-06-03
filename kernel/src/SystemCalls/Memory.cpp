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

#include <Memory/VMM.hpp>

#include <Scheduling/Process.hpp>
#include <Scheduling/Thread.hpp>

void* sys_mmap(void* addr, size_t length, int prot, int flags, sys_mmapExtraArgs* args) {
    if ((addr == nullptr && (flags & MAP_FIXED) > 0) || length == 0
            || prot <= PROT_NONE || prot > (PROT_READ | PROT_WRITE | PROT_EXEC)
            || flags <= 0 || flags > (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED) || (flags & (MAP_PRIVATE | MAP_ANONYMOUS)) == 0)
        return (void*)-EINVAL;

    if (prot == (PROT_WRITE | PROT_EXEC))
        return (void*)-ENOSYS;

    Thread* current = Thread::GetCurrentThread();
    Process* proc = current->GetParent();
    VMM::VMM* vmm = proc->GetVMM();
    if (vmm == nullptr)
        return (void*)-ENOSYS;

    // Don't bother with the extra args for now, but this is how the check would be done:
    // sys_mmapExtraArgs kArgs;
    // if (!UserRead(args, &kArgs, sizeof(sys_mmapExtraArgs), proc))
    //     return (void*)-EFAULT;

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

    void* mem = vmm->AllocatePages(pageCount, addr, protection, true);
    if (mem == nullptr && addr != nullptr && (flags & MAP_FIXED) == 0)
        mem = vmm->AllocatePages(pageCount, nullptr, protection, true);

    if (mem == nullptr)
        return (void*)((flags & MAP_FIXED) == 0 ? (int64_t)-ENOMEM : (int64_t)-EEXIST);

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

    return vmm->FreePages(addr, pageCount) ? ESUCCESS : -EINVAL;
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

    return vmm->RemapPages(addr, pageCount, protection, true) ? ESUCCESS : -EINVAL;
}
