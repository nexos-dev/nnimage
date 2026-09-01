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
#include "include/ImageTypes.h"

ResCustom<std::unique_ptr<Image>, ImageError> Image::ImageFactory (ImgType type, const std::string& name)
{
    assert (type < ImgType::Max);
    return factory[type](name);
}

ResCustom<std::unique_ptr<Image>, ImageError> Image::ImageFactory (const std::string& type,
    const std::string& name)
{
    ImgType imageType = ResolveType (type);
    if (imageType == ImgType::Max)
        return ImageError (ErrorCode::InvalidImgType, {{"type", type}, {"name", name}});
    return ImageFactory (imageType, name);
}

BackendType Image::GetBackendType (BackendType suggestion) const
{
    // We prefer to use the suggestion, then the default, and if all else fails, use the first valid and
    // enabled backend
    if (checkBackendForImage (suggestion))
        return suggestion;
    if (checkBackendForImage (BACKEND_DEFAULT))
        return BACKEND_DEFAULT;
    return firstAvailBackend();
}

ImageResult Image::Set (const std::string& name, const ImageVal& val)
{
    ImgProp prop = ResolveProp (name);
    if (prop == ImgProp::Max)
        return ImageError (ErrorCode::InvalidImgProp, {{"prop_name", name}, {"name", spec.name}});
    return Set (prop, val);
}

ImageResult Image::Set (ImgProp prop, const ImageVal& val)
{
    const auto& registry = getRegistry();
    auto it = registry.find (prop);
    if (it == registry.end())
    {
        it = baseRegistry.find (prop);
        if (it == baseRegistry.end())
        {
            // Usually this would occur if a user wants a property not available on a specific image type,
            // e.g., boot emulation on a GPT image
            return ImageError (ErrorCode::InvalidImgProp,
                {{"prop_name", GetPropName (prop)}, {"name", spec.name}});
        }
    }
    assert (it->second.setter);

    // Check the type
    const ImgConfItem& propReg = it->second;
    if (propReg.inputType != val.GetType())
    {
        // TODO: report the expected and found types. We just don't have a good way to get the type in string
        // form
        return ImageError (ErrorCode::PropTypeMismatch,
            {{"prop_name", GetPropName (prop)}, {"name", spec.name}});
    }

    return propReg.setter (*this, val);
}

ImageResult Image::Validate()
{
    // Check if spec has size unset
    if (spec.size < 0)
    {
        return ImageError (ErrorCode::MissingRequiredProp,
            {{"prop", ImgProp::Size}, {"image_ptr", this}, {"name", spec.name}});
    }

    if (spec.name.empty())
        spec.name = "nndisk";

    return Success();
}

ImageResult IsoImage::Validate()
{
    // Check for a boot image on emulated discs
    if (bootEmu != IsoBootEmu::Noemu)
    {
        if (bootImageName.empty())
            return ImageError (ErrorCode::ImgInvalid,
                "No boot image specified on image \"" + spec.name + "\"");
        // Ensure that the boot image is compatible
        assert (bootImage);
        if (bootImage->GetType() != bootEmuModes[bootEmu])
        {
            // TODO: print expected emulation
            return ImageError (ErrorCode::ImgInvalid,
                "Invalid boot emulation specified on image \"" + spec.name + "\"");
        }
    }
    return Image::Validate();
}

ImageResult FloppyImage::Validate()
{
    // Ensure this is a valid floppy size
    // NOTE: not all possible floppy sizes have been included but if a user needs a 360K floppy they have
    // bigger issues anyway. Quite frankly if they need a floppy to begin with that's already rather curious
    auto it = std::find (validSizes.begin(), validSizes.end(), spec.size);
    if (it == validSizes.end())
        return ImageError (ErrorCode::BadFloppySize, {{"name", spec.name}});
    return Image::Validate();
}

ImageResult Partition::Set (const std::string& name, const ImageVal& val)
{
    PartProp prop = ResolveName (name);
    if (prop == PartProp::Max)
        return ImageError (ErrorCode::InvalidPartProp, {{"prop_name", name}, {"name", spec.name}});
    return Set (prop, val);
}

ImageResult Partition::Set (PartProp prop, const ImageVal& val)
{
    auto it = registry.find (prop);
    if (it == registry.end())
    {
        // Shouldn't ever happen but then again we can't be too safe
        return ImageError (ErrorCode::InvalidPartProp,
            {{"prop_name", GetPropName (prop)}, {"name", spec.name}});
    }

    if (it->second.inputType != val.GetType())
        return ImageError (ErrorCode::PropTypeMismatch, {{"prop_name", GetPropName (prop)}});

    return it->second.setter (*this, val);
}

void ImageError::makeMessage (ErrorFrame& frame)
{
    std::stringstream msg;
    // Check if we have a file/line
    if (auto it = keys.find ("file"); it != keys.end())
        msg << getString (it) << ":";
    if (auto it = keys.find ("line"); it != keys.end())
        msg << getString (it) << ": ";
    else
        msg << " ";    // In case we have a file by itself with no line, it will still have a space at the end
                       // of it

    switch (frame.code)
    {
        // NOTE: all the below assertKeys calls only do anything on debug builds. That shouldn't be an issue
        case ErrorCode::NameMissing:
            assertKeys ({"block_type"});
            msg << "Name required for block type \"" << getString ("block_type") << "\"";
            break;
        case ErrorCode::InvalidImgType:
            assertKeys ({"type", "name"});
            msg << "Invalid image type \"" << getString ("type") << "\" specified on image \""
                << getString ("name") << "\"";
            break;
        case ErrorCode::MissingRequiredProp: {
            assertKeys ({"prop", "name", "image_ptr"});
            // HACK ALERT
            Image* img = getValue<Image*> ("image_ptr");
            assert (img);
            msg << "Missing required property \"" << img->GetPropName (getValue<ImgProp> ("prop"))
                << "\" on image \"" << getString ("name") << "\"";
            break;
        }
        case ErrorCode::InvalidImgProp:
            assertKeys ({"prop_name", "name"});
            msg << "Unrecognized property \"" << getString ("prop_name") << "\" specified on image \""
                << getString ("name") << "\"";
            break;
        case ErrorCode::BadFloppySize:
            assertKeys ({"name"});
            msg << "Floppy disc \"" << getString ("name") << "\" must have size 720K, 1.44M, or 2.88M";
            break;
        case ErrorCode::InvalidPartProp:
            assertKeys ({"prop_name", "name"});
            msg << "Unrecognized property \"" << getString ("prop_name") << "\" specified on partition \""
                << getString ("name") << "\"";
            break;
        case ErrorCode::PropTypeMismatch:
            assertKeys ({"prop_name"});
            msg << "Invalid type specified on property \"" << getString ("prop_name") << "\"";
            break;
        default:
            msg << frame.msg;
    }
    frame.msg = msg.str();
}

// Now begins the all-important registries

// NOTE: this contains names across all image types. It might be a questionable design choice but it's simpler
// then having to chase down 100 different registries when we already do enough of that here
const NameRegistry<ImgProp> Image::nameRegistry = {{"size", ImgProp::Size},
    {"boot_mode", ImgProp::BootMode},
    // MBR/GPT
    {"mbr_file", ImgProp::MbrFile},
    {"vbr_file", ImgProp::VbrFile},
    // ISO9660
    {"boot_emu", ImgProp::BootEmu},
    {"boot_image", ImgProp::BootImage}};

const EnumArray<ImgType, ImgConstruct, ImgType::Max> Image::factory = {
    {ImgType::Mbr,
        [] (const std::string& name) -> std::unique_ptr<Image> { return std::make_unique<MbrImage> (name); }},
    {ImgType::Gpt,
        [] (const std::string& name) -> std::unique_ptr<Image> { return std::make_unique<GptImage> (name); }},
    {ImgType::Iso9660,
        [] (const std::string& name) -> std::unique_ptr<Image> { return std::make_unique<IsoImage> (name); }},
    {ImgType::Floppy, [] (const std::string& name) -> std::unique_ptr<Image> {
         return std::make_unique<FloppyImage> (name);
     }}};

const NameRegistry<ImgType> Image::typeNames = {{"mbr", ImgType::Mbr},
    {"gpt", ImgType::Gpt},
    {"iso9660", ImgType::Iso9660},
    {"floppy", ImgType::Floppy}};

const NameRegistry<BootMode> Image::bootModes = {{"none", BootMode::None},
    {"bios", BootMode::Bios},
    {"efi", BootMode::Efi}};

// NOTE: when the day comes that C++26 is fully ratified and implemented, the first thing I'm doing is erasing
// this whole thing and using reflection to simplify this mess

// clang-format off
const ImgConfRegistry Image::baseRegistry = {
    {ImgProp::Size, {typeid (ImageNumId),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            img.spec.size = (*val.Get<ImageNumId>()).Get();
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return img.spec.size;
        }
    }},
    {ImgProp::BootMode, {typeid (ImageId), 
        [] (Image& img, const ImageVal& val) -> ImageResult {

            std::string id = std::string(*val.Get<ImageId>());
            auto mode = resolveBootMode (id);

            if (mode == BootMode::Max)
                return invalidId (img, id);

            img.spec.bootMode = mode;
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return img.spec.bootMode;
        }
    }}
};

// Image type-specific registrys

const ImgConfRegistry MbrImage::registry = {
    {ImgProp::VbrFile, {typeid (std::string),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            derived<MbrImage> (img).vbrFile = *val.Get<std::string>();
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return derived<MbrImage> (img).vbrFile;
        }
    }},
    {ImgProp::MbrFile, {typeid (std::string),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            derived<MbrImage> (img).mbrFile = *val.Get<std::string>();
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return derived<MbrImage> (img).mbrFile;
        }
    }}
};

const ImgConfRegistry GptImage::registry = {
    {ImgProp::VbrFile, {typeid (std::string),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            derived<GptImage> (img).vbrFile = *val.Get<std::string>();
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return derived<GptImage> (img).vbrFile;
        }
    }},
    {ImgProp::MbrFile, {typeid (std::string),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            derived<GptImage> (img).mbrFile = *val.Get<std::string>();
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return derived<GptImage> (img).mbrFile;
        }
    }}
};

const ImgConfRegistry IsoImage::registry = {
    {ImgProp::BootImage, {typeid (std::string),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            derived<IsoImage> (img).bootImageName = *val.Get<std::string>();
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return derived<IsoImage> (img).bootImageName;
        }
    }},
    {ImgProp::BootEmu, {typeid (ImageId),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            std::string id = std::string(*val.Get<ImageId>());
            IsoBootEmu bootEmu = IsoImage::bootEmus.Resolve (id);
            if (bootEmu == IsoBootEmu::Max)
                return invalidId (img, id);

            derived<IsoImage> (img).bootEmu = bootEmu;
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return derived<IsoImage> (img).bootEmu;
        }
    }}
};

const ImgConfRegistry FloppyImage::registry = {
    {ImgProp::MbrFile, {typeid (std::string),
        [] (Image& img, const ImageVal& val) -> ImageResult {
            derived<FloppyImage> (img).mbrFile = *val.Get<std::string>();
            return Success();
        },
        [] (Image& img) -> std::optional<std::any> {
            return derived<FloppyImage> (img).mbrFile;
        }
    }}
};

const EnumArray<IsoBootEmu, ImgType, IsoBootEmu::Max> IsoImage::bootEmuModes = {{IsoBootEmu::Noemu, ImgType::Max},
    {IsoBootEmu::Hdd, ImgType::Mbr},    // NOTE: A GPT image is technically valid, but we don't have a clean
                                        // way to represent that and if someone is dumb enough to use HDD
                                        // emulation that I'm not responsible for their descisions
    {IsoBootEmu::Fdd, ImgType::Floppy}};

const NameRegistry<IsoBootEmu> IsoImage::bootEmus = {
    {"noemu", IsoBootEmu::Noemu},
    {"fdd", IsoBootEmu::Fdd},
    {"hdd", IsoBootEmu::Hdd}
};

// Partition types registry

const NameRegistry<PartProp> Partition::nameRegistry = {{"start", PartProp::Start},
    {"size", PartProp::Size},
    {"format", PartProp::Format},
    {"prefix", PartProp::Prefix},
    {"is_boot", PartProp::IsBoot}};

const PartConfRegistry Partition::registry = {
    {PartProp::Start, {typeid (ImageNumId),
        [] (Partition& part, const ImageVal& val) -> ImageResult {
            part.spec.start = (*val.Get<ImageNumId>()).Get();
            return Success();
        },
        [] (Partition& part) -> std::optional<std::any> {
            return part.spec.start;
        }
    }},
    {PartProp::Size, {typeid (ImageNumId),
        [] (Partition& part, const ImageVal& val) -> ImageResult {
            part.spec.size = (*val.Get<ImageNumId>()).Get();
            return Success();
        },
        [] (Partition& part) -> std::optional<std::any> {
            return part.spec.size;
        }
    }},
    {PartProp::Format, {typeid (std::string),
        [] (Partition& part, const ImageVal& val) -> ImageResult {
            part.spec.format = *val.Get<std::string>();
            return Success();
        },
        [] (Partition& part) -> std::optional<std::any> {
            return part.spec.format;
        }
    }},
    {PartProp::Prefix, {typeid (std::string),
        [] (Partition& part, const ImageVal& val) -> ImageResult {
            part.spec.prefix = *val.Get<std::string>();
            return Success();
        },
        [] (Partition& part) -> std::optional<std::any> {
            return part.spec.prefix;
        }
    }},
    {PartProp::IsBoot, {typeid (bool),
        [] (Partition& part, const ImageVal& val) -> ImageResult {
            part.spec.isBoot = *val.Get<bool>();
            return Success();
        },
        [] (Partition& part) -> std::optional<std::any> {
            return part.spec.isBoot;
        }
    }}
};
