/*
    BootLoad.cxx - contains bootloader image component
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

#include "include/Image.h"
#include "include/comp/PartComp.h"

std::unique_ptr<BootLoadComp> BootLoadComp::Factory (BootLoadType type, Image& owner)
{
    return bootFactory[type](owner);
}

std::unique_ptr<BootLoadComp> BootLoadComp::Factory (std::string_view type, Image& owner)
{
    BootLoadType typeVal = nameRegistry.Resolve (type);
    if (typeVal == BootLoadType::Max)
        return nullptr;
    return bootFactory[typeVal](owner);
}

ImageResult BootLoadComp::Validate()
{
    return Success();
}

// clang-format off

const CompConfRegistry BootLoadComp::registry = {

};

const CompConfRegistry GrubBootComp::registry = {

};

const CompConfRegistry BootNoneComp::registry = {

};
