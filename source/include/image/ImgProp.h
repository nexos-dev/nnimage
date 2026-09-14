/*
    ImgProp.h - contains global property registry for all components
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

#ifndef IMGPROP_H
#define IMGPROP_H

#include "include/CompType.h"

#include <array>
#include <string_view>
#include <utility>

// X-Macro tables for image/partition property enums

// NOTE: each property has an owning component. This is for the slot level, as each
// property by definition is owned at that level so it can be redefined for different types across the same slot
#define IMG_PROP_TABLE(X)             \
    X (Size, "size", Max)             \
    X (BootMode, "boot_mode", Max)    \
    X (PartType, "type", Max)         \
    X (BootLoad, "bootloader", Max)   \
    X (MbrFile, "mbr_file", Boot)     \
    X (VbrFile, "vbr_file", Boot)     \
    X (BootEmu, "boot_emu", PartType) \
    X (BootImage, "boot_image", PartType)

#define PART_PROP_TABLE(X) \
    X (Start, "start")     \
    X (Size, "size")       \
    X (Format, "format")   \
    X (Prefix, "prefix")   \
    X (IsBoot, "is_boot")

#define IMG_ENUM_ENTRY(name, str, comp) name,
#define PART_ENUM_ENTRY(name, str)      name,

enum class ImgProp
{
    IMG_PROP_TABLE (IMG_ENUM_ENTRY) Max
};

enum class PartProp
{
    PART_PROP_TABLE (PART_ENUM_ENTRY) Max
};

#undef IMG_ENUM_ENTRY
#undef PART_ENUM_ENTRY

#define IMG_PROP_ENTRY(name, str, comp) {str, ImgProp::name},
#define PART_PROP_ENTRY(name, str)      {str, PartProp::name},

// Builds an array from the X-Macro table so that the name registries are evaluated at build-time
consteval auto MakeImgPropRegistry()
{
    return std::to_array<std::pair<std::string_view, ImgProp>> ({IMG_PROP_TABLE (IMG_PROP_ENTRY)});
}

consteval auto MakePartPropRegistry()
{
    return std::to_array<std::pair<std::string_view, PartProp>> ({PART_PROP_TABLE (PART_PROP_ENTRY)});
}

#undef IMG_PROP_ENTRY
#undef PART_PROP_ENTRY

// Create entry for component owner array
#define IMG_COMP_ENTRY(name, str, comp) {ImgProp::name, CompType::comp},

consteval auto MakeImgPropOwnerTable()
{
    return std::to_array<std::pair<ImgProp, CompType>> ({IMG_PROP_TABLE (IMG_COMP_ENTRY)});
}

#undef IMG_COMP_ENTRY

#endif
