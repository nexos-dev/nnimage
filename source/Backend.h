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

#ifndef BACKEND_H
#define BACKEND_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include "nnimage.h"

enum class BackendType
{
    None,
    Krun,
    Xorriso,
    Guestfs,
    Loopback,
    Max
};

class Task;
class ImgSpec;
class Backend
{
  public:
    static std::unique_ptr<Backend> BackendFactory (BackendType type);
    // Resolves backend type from string name
    static BackendType ResolveBackend (const std::string& name);
    // Gets a backend name from type
    static const std::string GetBackendName (BackendType type);
    // Functions for each backend function
    virtual std::unique_ptr<Task> CreateImage (const ImgSpec& spec, const std::string& fileName);
    virtual std::unique_ptr<Task> CreatePartTable (const ImgSpec& spec,
                                                   const std::string& fileName);
    bool BackendCreated()
    {
        return backendCreated;
    }
    static bool IsBackendEnabled (BackendType type);
    virtual ~Backend() = default;

  protected:
    bool backendCreated = false;

  private:
    const static std::unordered_map<std::string, BackendType> registry;
};

#include "backend/KrunBackend.h"
#include "backend/XorrisoBackend.h"
#include "backend/LoopbackBackend.h"
#include "backend/GuestfsBackend.h"

#endif
