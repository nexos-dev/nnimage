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
#include <algorithm>

// TODO: move most of these tables to ImageTypes.h

// clang-format off

// Property name registry
// NOTE: this contains the registry for all image types
// Maybe this should be changed?
const std::unordered_map<std::string, ImgProp> Image::nameRegistry = {
    {"size", ImgProp::Size},
    {"bootmode", ImgProp::BootMode},
    // ISO9660
    {"bootimage", ImgProp::BootImage},
    {"bootemu", ImgProp::BootEmu}
};

// Image registration table
const std::unordered_map<ImgProp, ImgConfItem> Image::baseRegistry = {
    {ImgProp::Size, {ConfType::NumId,
     [] (Image& img, const ConfVal& val, ConfErrorType& e) {
        int64_t size = Image::NormalizeNumId (val.GetNumId());
        if (size == -1)
            e = ConfErrorType::BadNumId;
        else
            img.spec.size = size;
      }
    }},

    {ImgProp::BootMode, {ConfType::Id, 
     [] (Image& img, const ConfVal& val, ConfErrorType& e) {
        const std::string& modeName = val.GetString();
        img.spec.bootMode = img.getBootMode (modeName);
        if (img.spec.bootMode == BootMode::Error)
            e = ConfErrorType::UnrecognizedId;
      }
    }}
};
// clang-format on

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

void Image::SetConf (const std::string& name, const ConfVal& val, ConfError& e)
{
    // Try to find property
    ImgProp prop = ResolveProp (name);
    if (prop == ImgProp::None)
    {
        e = ConfError (ConfErrorType::BadProp, name, val.GetLine());
        return;
    }
    SetConf (prop, val, e);
}

void Image::SetConf (ImgProp key, const ConfVal& val, ConfError& e)
{
    // Find in sub registry first
    auto it = getRegistry().find (key);
    if (it == getRegistry().end())
    {
        // Now look in base registry if non-existant in sub registry
        it = baseRegistry.find (key);
        if (it == baseRegistry.end())
        {
            e = ConfError (ConfErrorType::BadProp, getPropName (key), val.GetLine());
            return;
        }
    }
    ConfErrorType result = ConfErrorType::Ok;
    // Check the type
    if (!val.IsType (it->second.type))
    {
        e = ConfError (ConfErrorType::WrongType, getPropName (key), val.GetLine());
        return;
    }
    it->second.setter (*this, val, result);
    if (result != ConfErrorType::Ok)
    {
        // Generally, the error message is the property name
        // But for UnrecognizedId, its the value
        // TODO: maybe move this to setters?
        if (result == ConfErrorType::UnrecognizedId)
            e = ConfError (result, val.GetString(), val.GetLine());
        else
            e = ConfError (result, getPropName (key), val.GetLine());
    }
}

ImgProp Image::ResolveProp (const std::string& name)
{
    ImgProp prop = ImgProp::None;
    auto it = Image::nameRegistry.find (name);
    if (it != Image::nameRegistry.end())
        prop = it->second;
    return prop;
}

// TODO: use a lookup table for image type number-to-name
const std::string Image::GetTypeName (ImageType type)
{
    switch (type)
    {
        case ImageType::Floppy:
            return "floppy";
        case ImageType::Mbr:
            return "mbr";
        case ImageType::Gpt:
            return "gpt";
        case ImageType::Iso9660:
            return "iso9660";
        default:
            return "";
    }
}

// Use sparingly, has poor performance
const std::string Image::getPropName (ImgProp prop)
{
    // Use find_if to find
    auto it = std::find_if (
        Image::nameRegistry.begin(),
        Image::nameRegistry.end(),
        [&prop] (const std::pair<std::string, ImgProp> pair) { return pair.second == prop; });
    if (it == Image::nameRegistry.end())
        return "";
    return it->first;
}

bool Image::ResolvePartitions (ConfError& e)
{
    // Go through every partiiton reference and resolve it
    for (const PartRef& partRef : partitionNames)
    {
        auto part = GetAction()->GetPartition (partRef.GetName());
        if (!part)
        {
            e = ConfError (ConfErrorType::UnrecognizedId, partRef.GetName(), partRef.GetLine());
            return false;
        }
        AddPartition (std::move (part));
    }
    return true;
}

bool Image::Validate()
{
    // Make sure a size was passed
    if (spec.size == -1)
    {
        _log->Error ("image \"" + spec.name + "\" missing required property \"size\"");
        return false;
    }
    // Ensure there is a name
    if (spec.name.empty())
        spec.name = "nndisk";
    return true;
}

BackendType Image::GetBackendType (BackendType suggestion) const
{
    // First check the suggestion
    if (checkBackendForImage (suggestion))
        return suggestion;
    // Now check the default
    if (checkBackendForImage (BACKEND_DEFAULT))
        return BACKEND_DEFAULT;
    // Now just get the first valid backend
    // If we can't find one, it will return BackendType::None
    return firstAvailBackend();
}

// MBR image implementation

// MBR registry table
const std::unordered_map<ImgProp, ImgConfItem> MbrImage::registry = {};

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
const std::unordered_map<ImgProp, ImgConfItem> GptImage::registry = {};

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

const std::unordered_map<std::string, IsoBootEmu> IsoImage::bootEmus = {
    {"noemu", IsoBootEmu::Noemu},
    {"hdd", IsoBootEmu::Hdd},
    {"fdd", IsoBootEmu::Fdd}};

const std::unordered_map<IsoBootEmu, ImageType> IsoImage::bootEmuModes = {
    {IsoBootEmu::Noemu, ImageType::Error},
    {IsoBootEmu::Fdd, ImageType::Floppy},
    {IsoBootEmu::Hdd, ImageType::Mbr}};

// clang-format off
// ISO registry table
const std::unordered_map<ImgProp, ImgConfItem> IsoImage::registry = {
    {ImgProp::BootEmu, {ConfType::Id, 
     [] (Image& img, const ConfVal& val, ConfErrorType& e) {
        // Get img as IsoImage
        IsoImage& isoImg = dynamic_cast<IsoImage&> (img);
        // Get number from string
        auto it = bootEmus.find (val.GetString());
        if (it == bootEmus.end())
        {
            e = ConfErrorType::UnrecognizedVal;
            return;
        }
        isoImg.bootEmu = it->second;
      }
    }},
    {ImgProp::BootImage, {ConfType::Id,
     [] (Image& img, const ConfVal& val, ConfErrorType& e) {
        // Set it
        IsoImage& isoImg = dynamic_cast<IsoImage&> (img);
        isoImg.bootImageName = val.GetString();
      }
    }}
};
// clang-format on

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

bool IsoImage::Validate()
{
    // Make sure a boot image was passed
    if (bootEmu != IsoBootEmu::Noemu)
    {
        if (bootImageName.empty())
        {
            _log->Error ("image \"" + spec.name + "\" missing required property \"bootimage\"");
            return false;
        }
        // Resolve it
        bootImage = GetAction()->FindImage (bootImageName);
        if (bootImage == nullptr)
        {
            _log->Error ("non-existant image \"" + bootImageName +
                         "\" given as boot image on image \"" + spec.name + "\"");
            return false;
        }
        // Get the type
        ImageType type = IsoImage::bootEmuModes.find (bootEmu)->second;
        if (bootImage->GetType() != type)
        {
            _log->Error ("image \"" + spec.name + "\" requires image type \"" +
                         Image::GetTypeName (type) + "\" for boot image");
            return false;
        }
    }
    // Make a default name
    if (spec.name.empty())
        spec.name = "nncdrom";
    return Image::Validate();
}

// Floppy image implementation

// Floppy registry table
const std::unordered_map<ImgProp, ImgConfItem> FloppyImage::registry = {};

// Defined boot modes
const std::unordered_map<std::string, BootMode> FloppyImage::validBootModes = {
    {"none", BootMode::None},
    {"bios", BootMode::Bios}};

// Valid sizes for a floppy image
const std::vector<int> FloppyImage::validSizes = {720, 1440, 2880};

BootMode FloppyImage::getBootMode (const std::string& modeStr)
{
    auto it = validBootModes.find (modeStr);
    if (it == validBootModes.end())
        return BootMode::Error;
    return it->second;
}

bool FloppyImage::Validate()
{
    // Make sure this is a valid floppy disk size
    auto it = std::find (FloppyImage::validSizes.begin(),
                         FloppyImage::validSizes.end(),
                         spec.size / 1024);
    if (it == FloppyImage::validSizes.end())
    {
        _log->Error ("floppy image must be of size 720K, 1440K, or 2880K");
        return false;
    }
    return Image::Validate();
}

// Partition class
// clang-format off
const std::unordered_map<std::string, PartProp> Partition::nameRegistry = {
    {"format", PartProp::Format},
    {"prefix", PartProp::Prefix},
    {"start", PartProp::Start},
    {"size", PartProp::Size},
    {"isboot", PartProp::IsBoot}
};

const std::unordered_map<PartProp, PartConfItem> Partition::registry = {
    {PartProp::Format, {ConfType::String,
     [] (Partition& part, const ConfVal& val, ConfErrorType& e) 
        { part.spec.fs = val.GetString(); }
    }},

    {PartProp::Prefix, {ConfType::String,
     [] (Partition& part, const ConfVal& val, ConfErrorType& e) 
        { part.spec.prefix = val.GetString(); }
    }},

    {PartProp::Start, {ConfType::NumId,
     [] (Partition& part, const ConfVal& val, ConfErrorType& e) {
        const ConfNumId& start = val.GetNumId();
        part.spec.start = Image::NormalizeNumId (start);
        if (part.spec.start == -1)
            e = ConfErrorType::BadNumId;
      }
    }},

    {PartProp::Size, {ConfType::NumId,
     [] (Partition& part, const ConfVal& val, ConfErrorType& e) {
        const ConfNumId& size = val.GetNumId();
        part.spec.size = Image::NormalizeNumId (size);
        if (part.spec.size == -1)
            e = ConfErrorType::BadNumId;
       }
    }},

    {PartProp::IsBoot, {ConfType::Id,
     [] (Partition& part, const ConfVal& val, ConfErrorType& e) {
        if (!val.IsBool())
        {
            e = ConfErrorType::WrongType;
            return;
        }
        part.spec.isBoot = val.GetBoolean();
      }
    }}
};
// clang-format on

const std::string Partition::getPropName (PartProp prop)
{
    // Use find_if to find
    auto it = std::find_if (
        Partition::nameRegistry.begin(),
        Partition::nameRegistry.end(),
        [&prop] (const std::pair<std::string, PartProp> pair) { return pair.second == prop; });
    if (it == Partition::nameRegistry.end())
        return "";
    return it->first;
}

PartProp Partition::ResolveName (const std::string& name)
{
    // Find the name
    auto it = Partition::nameRegistry.find (name);
    if (it == Partition::nameRegistry.end())
        return PartProp::None;
    return it->second;
}

void Partition::SetConf (const std::string& name, const ConfVal& val, ConfError& e)
{
    // Find name
    PartProp prop = ResolveName (name);
    if (prop == PartProp::None)
    {
        e = ConfError (ConfErrorType::BadProp, name, val.GetLine());
        return;
    }
    SetConf (prop, val, e);
}

void Partition::SetConf (PartProp key, const ConfVal& val, ConfError& e)
{
    auto it = registry.find (key);
    if (it == registry.end())
    {
        e = ConfError (ConfErrorType::BadProp, getPropName (key), val.GetLine());
        return;
    }

    ConfErrorType result;
    // Check type
    if (!val.IsType (it->second.type))
    {
        e = ConfError (ConfErrorType::WrongType, getPropName (key), val.GetLine());
        return;
    }
    it->second.setter (*this, val, result);
    if (result != ConfErrorType::Ok)
        e = ConfError (result, getPropName (key), val.GetLine());
}
