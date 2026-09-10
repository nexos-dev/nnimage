/*
    Krun.cxx - Krun backend implementation
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

#include "backend/KrunBackend.h"
#include "include/SysMemory.h"
#include <libkrun.h>
#include <string.h>

#include <algorithm>
#include <thread>

KrunBackend::KrunBackend()
{
    // Create a log stream so that errors are properly logged
    /*int stream = _log->CreateLogStream ("libkrun error:", LogLevel::Error);
    if (stream == -1)
    {
        backendCreated = false;
        return;
    }
    // Redirect libkrun's log to our log stream
    krun_init_log (stream, KRUN_LOG_LEVEL_ERROR, KRUN_LOG_STYLE_AUTO, 0);*/
    // Create the krun context
    krunCtx = krun_create_ctx();
    if (krunCtx == -1)
    {
        _log->Error ("failed to create krun context");
        backendCreated = false;
        return;
    }
    // Ensure block and net devices are enabled
    bool blockEnabled = krun_has_feature (KRUN_FEATURE_BLK) == 1;
    bool netEnabled = krun_has_feature (KRUN_FEATURE_NET) == 1;
    // Handle failures
    if (!blockEnabled)
        _log->Error ("libkrun was built without block device support");
    if (!netEnabled)
        _log->Error ("libkrun was built without network device support");
    if (!blockEnabled || !netEnabled)
    {
        _log->Error ("one or more required features are not enabled in libkrun");
        backendCreated = false;
        return;
    }
    // Get the number of vCPUs to use
    // TODO: implement a way to specify the number of vCPUs to use for krun
    // I could do a command line argument, but that would require a whole new class of options to be
    // added I could also do an environ value
    int maxCpus = std::thread::hardware_concurrency();
    // Use half of the available CPUs for krun, but at least 1
    int krunCpus = std::max (maxCpus / 2, 1);
    // Get the amount of RAM the host has and use half of it for krun, but at least 512MB
    // TODO: definitely need to make this configurable, but for now this is fine
    uint64_t hostMemMB = GetMemorySizeMB();
    uint64_t krunMemMB = std::max (hostMemMB / 2, static_cast<uint64_t> (512));
    // Set it
    if (krun_set_vm_config (krunCtx, krunCpus, krunMemMB) == -1)
    {
        _log->Error ("failed to set krun VM config");
        backendCreated = false;
        return;
    }
    // Now we need to set the root filesystem
    backendCreated = true;
}

KrunBackend::~KrunBackend()
{
    if (krunCtx != -1)
        krun_free_ctx (krunCtx);
}

bool KrunBackend::AddImage (Image& img, const std::string& fileName, bool readonly)
{
    const auto& spec = img.GetSpec();
    // Get the block device
    std::string blockDev = blockDevGen++;
    // Call the API
    if (krun_add_disk2 (krunCtx, blockDev.c_str(), fileName.c_str(), KRUN_DISK_FORMAT_RAW, readonly) == -1)
    {
        _log->Error ("failed to add disk \"" + spec.name + "\" to krun");
        return false;
    }
    // Now set the tag
    img.SetBackendTag (blockDev);
    return true;
}

std::unique_ptr<Task> KrunBackend::CreatePartTable (Image& img, const std::string& fileName)
{
    auto taskCb = [this, &img, fileName]() {
        const auto& spec = img.GetSpec();
        _log->Error ("i want to fail");
        return false;
    };
    const auto& spec = img.GetSpec();
    auto task = std::make_unique<Task> (taskCb,
        "CreatePartTable",
        "Creating partition table for image \"" + spec.name + "\"...");
    return task;
}
