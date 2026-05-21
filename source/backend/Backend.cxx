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

// Backend list
// clang-format off
const std::unordered_map<std::string, BackendType> Backend::registry = {
    {"krun", BackendType::Krun},
    {"isofs", BackendType::Isofs},
    {"loopback", BackendType::Loopback},
    {"guestfs", BackendType::Guestfs}  
};
// clang-format on

BackendType Backend::ResolveBackend (const std::string& name)
{
    auto it = Backend::registry.find (name);
    if (it == Backend::registry.end())
        return BackendType::None;
    return it->second;
}
