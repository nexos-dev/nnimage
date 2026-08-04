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

#include <cstdint>
#include <unistd.h>

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
    long free_pages = sysconf (_SC_AVPHYS_PAGES);
    long page_size = sysconf (_SC_PAGE_SIZE);
    return static_cast<uint64_t> (free_pages) * static_cast<uint64_t> (page_size);
}

static uint64_t GetFreeMemorySizeMB()
{
    return GetFreeMemorySize() / (1024 * 1024);
}
