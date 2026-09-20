/*
    Backend.h - contains backend classes
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

#ifndef NNIMAGE_BACKEND_H
#define NNIMAGE_BACKEND_H

#include "include/Task.h"
#include "include/EnumArray.h"
#include "include/Image.h"
#include "BackendTypes.h"

#include <memory>
#include <string>
#include <string_view>

class Backend
{
  public:
    static std::unique_ptr<Backend> BackendFactory (BackendType type);
    static BackendType ResolveBackend (std::string_view name);
    static std::string_view GetBackendName (BackendType type);
    bool BackendCreated()
    {
        return backendCreated;
    }
    // Functions for each backend function
    virtual std::unique_ptr<Task> CreateImage (Image& img, std::string_view fileName);
    virtual std::unique_ptr<Task> CreatePartTable (Image& img, std::string_view fileName);
    virtual bool AddImage (Image& img, std::string_view fileName, bool readonly) = 0;
    static bool IsBackendEnabled (BackendType type);
    virtual ~Backend() = default;

  protected:
    bool backendCreated = false;
};

#include "Backends.h"

#endif
