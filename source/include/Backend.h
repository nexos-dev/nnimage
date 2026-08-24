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
#include "BackendTypes.h"

class Image;
class Backend
{
  public:
    static std::unique_ptr<Backend> BackendFactory (BackendType type);
    static BackendType ResolveBackend (const std::string& name);
    static const std::string GetBackendName (BackendType type);
    bool BackendCreated()
    {
        return backendCreated;
    }
    // Functions for each backend function
    virtual std::unique_ptr<Task> CreateImage (Image& img, const std::string& fileName);
    virtual std::unique_ptr<Task> CreatePartTable (Image& img, const std::string& fileName);
    virtual bool AddImage (Image& img, const std::string& fileName, bool readonly) = 0;
    static bool IsBackendEnabled (BackendType type);
    virtual ~Backend() = default;

  protected:
    bool backendCreated = false;
};

#endif
