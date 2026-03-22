/*
    Image.cxx - contains image handling code
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

// Image registration table
const std::unordered_map<std::string, Setter> Image::baseRegistry = {
    // size
    {"size",
     [] (Image& img, ImageConf& conf, const ParseVal& val, ConfErrorType& e) {
         if (!val.IsType (PropType::NumId))
         {
             e = ConfErrorType::WrongType;
             return;
         }
         int64_t size = Image::NormalizeNumId (val.GetNumId());
         if (size == -1)
             e = ConfErrorType::BadNumId;
         else
             img.size = size;
     }},
    {"bootmode", [] (Image& img, ImageConf& conf, const ParseVal& val, ConfErrorType& e) {
         if (!val.IsType (PropType::Id))
         {
             e = ConfErrorType::WrongType;
             return;
         }
         // Now find it in the hash table
     }}};

std::unique_ptr<Image> Image::ImageFactory (const std::string& type, const std::string& name)
{
    // Figure out what kind of image this is
    // TODO: maybe we should use a registry table of some sort?
    if (type == "mbr")
        return std::make_unique<MbrImage> (name);
    else if (type == "gpt")
        return std::make_unique<GptImage> (name);
    else if (type == "iso9660")
        return std::make_unique<IsoImage> (name);
    else if (type == "floppy")
        return std::make_unique<FloppyImage> (name);
    return nullptr;
}

// TODO use a table
int64_t Image::NormalizeNumId (const ConfNumId& numId)
{
    // Figure out the multipler
    if (numId.id == "B")
        return numId.num;
    else if (numId.id == "KiB")
        return numId.num * 1024;
    else if (numId.id == "MiB")
        return numId.num * (1024 * 1024);
    else if (numId.id == "GiB")
        return numId.num * (1024 * 1024 * 1024);
    else if (numId.id == "TiB")
        return numId.num * ((int64_t) 1024 * 1024 * 1024 * 1024);
    return -1;
}

void Image::SetConf (const ParseProp& prop, const std::string& name, ImageConf& conf, ConfError& e)
{
    // Find in base registry table
    auto it = baseRegistry.find (name);
    if (it == baseRegistry.end())
    {
        e = ConfError (ConfErrorType::BadProp, prop);
        return;
    }
    ConfErrorType result;
    ParseVal val;
    conf.GetVal (prop, val, 0);
    it->second (*this, conf, val, result);
    if (result != ConfErrorType::Ok)
        e = ConfError (result, prop);
}

// MBR image implementation

// MBR registry table
const std::unordered_map<std::string, Setter> MbrImage::registry = {};

// Defined boot modes
const std::unordered_map<std::string, BootMode> MbrImage::validBootModes = {
    {"none", BootMode::None},
    {"bios", BootMode::Bios}};

BootMode MbrImage::getBootMode (const std::string& modeStr)
{
    auto it = validBootModes.find (modeStr);
    if (it == validBootModes.end())
        return BootMode::Error;
    return it->second;
}

// GPT image implementation

// GPT registry table
const std::unordered_map<std::string, Setter> GptImage::registry = {};

// Defined boot modes
const std::unordered_map<std::string, BootMode> GptImage::validBootModes = {
    {"none", BootMode::None},
    {"bios", BootMode::Bios},
    {"efi", BootMode::Efi}};

BootMode GptImage::getBootMode (const std::string& modeStr)
{
    auto it = validBootModes.find (modeStr);
    if (it == validBootModes.end())
        return BootMode::Error;
    return it->second;
}

// ISO image implementation

// ISO registry table
const std::unordered_map<std::string, Setter> IsoImage::registry = {};

// Defined boot modes
const std::unordered_map<std::string, BootMode> IsoImage::validBootModes = {
    {"none", BootMode::None},
    {"bios", BootMode::Bios},
    {"efi", BootMode::Efi}};

BootMode IsoImage::getBootMode (const std::string& modeStr)
{
    auto it = validBootModes.find (modeStr);
    if (it == validBootModes.end())
        return BootMode::Error;
    return it->second;
}

// Floppy image implementation

// Floppy registry table
const std::unordered_map<std::string, Setter> FloppyImage::registry = {};

// Defined boot modes
const std::unordered_map<std::string, BootMode> FloppyImage::validBootModes = {
    {"none", BootMode::None},
    {"bios", BootMode::Bios}};

BootMode FloppyImage::getBootMode (const std::string& modeStr)
{
    auto it = validBootModes.find (modeStr);
    if (it == validBootModes.end())
        return BootMode::Error;
    return it->second;
}
