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

#include "nnimage.h"
#include <libkrun.h>
#include <string.h>

KrunBackend::KrunBackend()
{
    // Create a log stream so that errors are properly logged
    int stream = _log->CreateLogStream ("libkrun error:", LogLevel::Error);
    if (stream == -1)
    {
        backendCreated = false;
        return;
    }
    // Redirect libkrun's log to our log stream
    krun_init_log (stream, KRUN_LOG_LEVEL_ERROR, KRUN_LOG_STYLE_AUTO, 0);
    // Create the krun context
    krunCtx = krun_create_ctx();
    backendCreated = true;
}

KrunBackend::~KrunBackend()
{
    if (krunCtx != -1)
        krun_free_ctx (krunCtx);
}

std::unique_ptr<Task> KrunBackend::CreatePartTable (const ImgSpec& spec,
                                                    const std::string& fileName)
{
    auto taskCb = [this, &spec, fileName]() {
        // Lock it so we don't have multiple threads writing to the same file at the same time
        std::lock_guard<std::mutex> guard (spec.lock);
        _log->Error ("i want to fail");
        return false;
    };
    auto task = std::make_unique<Task> (taskCb, "CreatePartTable");
    return task;
}
