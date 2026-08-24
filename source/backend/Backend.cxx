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
#include "BackendTable.h"

BackendType Backend::ResolveBackend (const std::string& name)
{
    auto it = BackendNameRegistry.find (name);
    // TODO: should communicate the error better
    if (it == BackendNameRegistry.end())
        return BackendType::None;
    return it->second;
}

const std::string Backend::GetBackendName (BackendType type)
{
    auto it = std::find_if (BackendNameRegistry.begin(),
                            BackendNameRegistry.end(),
                            [type] (const auto& pair) { return pair.second == type; });
    if (it == BackendNameRegistry.end())
        return "unknown";
    return it->first;
}

std::unique_ptr<Backend> Backend::BackendFactory (BackendType type)
{
    auto factory = BackendFactoryRegistry[type];
    return (factory != nullptr) ? factory() : nullptr;
}

std::unique_ptr<Task> Backend::CreateImage (Image& img, const std::string& fileName)
{}

std::unique_ptr<Task> Backend::CreatePartTable (Image& img, const std::string& fileName)
{
    return Task::EmptyTask();
}

bool Backend::IsBackendEnabled (BackendType type)
{
    // Just check if the factory registry has a valid entry for this backend type
    auto factory = BackendFactoryRegistry[type];
    return factory != nullptr;
}
