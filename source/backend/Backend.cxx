/*
    Backend.cxx - contains global backend interface
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

#include "nnimage.h"
#include <cstdio>

// Backend list
// clang-format off
const std::unordered_map<std::string, BackendType> Backend::registry = {
    {"krun", BackendType::Krun},
    {"xorriso", BackendType::Xorriso},
    {"loopback", BackendType::Loopback},
    {"guestfs", BackendType::Guestfs},
};
// clang-format on

BackendType Backend::ResolveBackend (const std::string& name)
{
    auto it = Backend::registry.find (name);
    if (it == Backend::registry.end())
        return BackendType::None;
    return it->second;
}

const std::string Backend::GetBackendName (BackendType type)
{
    return std::find_if (Backend::registry.begin(),
                         Backend::registry.end(),
                         [type] (const auto& pair) { return pair.second == type; })
        ->first;
}

std::unique_ptr<Backend> Backend::BackendFactory (BackendType type)
{
    switch (type)
    {
        case BackendType::Krun:
            return std::make_unique<KrunBackend>();
#ifdef USE_XORRISO
        case BackendType::Xorriso:
            return std::make_unique<XorrisoBackend>();
#endif
#ifdef USE_LOOPBACK
        case BackendType::Loopback:
            return std::make_unique<LoopbackBackend>();
#endif
#ifdef HAVE_GUESTFS
        case BackendType::Guestfs:
            return std::make_unique<GuestfsBackend>();
#endif
        default:
            return nullptr;
    }
}

std::unique_ptr<Task> Backend::CreateImage (const ImgSpec& spec, const std::string& fileName)
{
    auto taskCb = [this, &spec, fileName]() {
        // Lock it so we don't have multiple threads writing to the same file at the same time
        std::lock_guard<std::mutex> guard (spec.lock);
        size_t sz = spec.size;
        assert (sz > 0);
        // Create the file
        FILE* imgFile = std::fopen (fileName.c_str(), "wb");
        if (imgFile == nullptr)
        {
            _log->SysError ("failed to create image file \"" + fileName + "\"");
            return false;
        }
        // Now write the file with zeroes
        // TODO: dynamically sized buffer
        const size_t bufSize = 1024 * 1024;
        std::vector<char> buf (bufSize, 0);
        while (sz > 0)
        {
            size_t writeSize = std::min (sz, bufSize);
            size_t written = std::fwrite (buf.data(), 1, writeSize, imgFile);
            if (written != writeSize)
            {
                _log->SysError ("failed to write to image file \"" + fileName + "\"");
                std::fclose (imgFile);
                return false;
            }
            sz -= written;
        }
        std::fclose (imgFile);
        return true;
    };
    auto task = std::make_unique<Task> (taskCb, "CreateImage");
    return task;
}

std::unique_ptr<Task> Backend::CreatePartTable (const ImgSpec& spec, const std::string& fileName)
{
    return Task::EmptyTask();
}

bool Backend::IsBackendEnabled (BackendType type)
{
    switch (type)
    {
        case BackendType::Krun:
            return true;
        case BackendType::Xorriso:
#ifdef USE_XORRISO
            return true;
#else
            return false;
#endif
        case BackendType::Loopback:
#ifdef USE_LOOPBACK
            return true;
#else
            return false;
#endif
        case BackendType::Guestfs:
#ifdef HAVE_GUESTFS
            return true;
#else
            return false;
#endif
        default:
            return false;
    }
}
