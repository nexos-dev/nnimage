/*
    SysMemory.h - contains functions to get host's memory size
    Copyright 2026 Jedidiah Thompson

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <unistd.h>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

#include <cstdint>

static uint64_t GetMemorySize()
{
    long pages = sysconf (_SC_PHYS_PAGES);
    long page_size = sysconf (_SC_PAGE_SIZE);
    return static_cast<uint64_t> (pages) * static_cast<uint64_t> (page_size);
}

static uint64_t GetMemorySizeMB()
{
    return GetMemorySize() / (1024 * 1024);
}

static uint64_t GetFreeMemorySize()
{
#ifndef __APPLE__
    long free_pages = sysconf (_SC_AVPHYS_PAGES);
    long page_size = sysconf (_SC_PAGE_SIZE);
    return static_cast<uint64_t> (free_pages) * static_cast<uint64_t> (page_size);
#else
    // On Apple systems, use vm_statistics64 to get free memory
    vm_size_t pgSize;
    mach_port_t host = mach_host_self();
    vm_statistics64_t stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    host_page_size (host, &pgSize);

    if (host_statistics64 (host, HOST_VM_INFO64, (host_info64_t) &stats, &count) != KERN_SUCCESS)
        throw std::runtime_error ("unable to get host free memory");    // Shouldn't happen, but who knows what arcane
                                                                        // mach failure paths exist
    return (stats->free_count + stats->inactive_count) * pgSize;

#endif
}

static uint64_t GetFreeMemorySizeMB()
{
    return GetFreeMemorySize() / (1024 * 1024);
}
