/*
    PartType.cxx - contains partition type components
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

std::unique_ptr<PartTypeComp> PartTypeComp::Factory (PartType type, Image& owner)
{
    return factory[type](owner);
}

std::unique_ptr<PartTypeComp> PartTypeComp::Factory (std::string_view type, Image& owner)
{
    PartType typeVal = nameRegistry.Resolve (type);
    if (typeVal == PartType::Max)
        return nullptr;
    return factory[typeVal](owner);
}

ImageResult PartTypeComp::Validate()
{
    return Success();
}

ImageResult IsoPartComp::Validate()
{
    assert (bootEmu != IsoBootEmu::Max);
    // If use boot emulation other than noemu, check that an image was provided and resolved
    if (bootEmu != IsoBootEmu::NoEmu)
    {
        if (bootImageName.empty())
        {
            // TODO: custom ImageError handlers
            return ImageError (ErrorCode::ImgInvalid, "ISO9660 image missing boot image");
        }
        assert (bootImage);

        // Ensure boot image type is valid
        auto res = bootImage->GetComponent<PartTypeComp> (CompType::PartType);
        if (!res.IsOk())
        {
            if (!owner.GetName().empty())
            {
                res.GetError().Add ({ErrorDomain::ImageConf, ErrorCode::ImgInvalid},
                    "Failed to query boot image for image \"{}\"",
                    owner.GetName());
            }
            return res.GetError();
        }
        PartTypeComp* bootImgPart = res.GetValue();
        PartType type = bootImgPart->GetPartType();

        auto& validTypes = validBootImage[bootEmu];
        auto it = std::find (validTypes.begin(), validTypes.end(), type);
        if (it == validTypes.end())
        {
            // TODO better diagnostic here
            return ImageError (ErrorCode::ImgInvalid,
                "Partition layout \"" + getTypeName (type) +
                    "\" is invalid for ISO9660 image given specified boot emulation");
        }
    }
    else
    {
        // Noemu can't accept a boot image
        if (!bootImageName.empty())
        {
            return ImageError (ErrorCode::ImgInvalid,
                "ISO9660 image can't take boot image given specified boot emulation");
        }
    }
    return Success();
}

// Registries
// clang-format off

const CompConfRegistry PartTypeComp::registry = {

};

const CompConfRegistry MbrPartComp::registry = {

};

const CompConfRegistry GptPartComp::registry = {

};

// ISO9660 registry
const CompConfRegistry IsoPartComp::registry = {
    {ImgProp::BootEmu, 
        {typeid(ImageId),
            ImageId ("noemu"),
            [] (Component& comp, const ImageVal& val) -> ImageResult
            {
                IsoPartComp& isoComp = derived<IsoPartComp> (comp);
                // Resolve the ID
                std::string name = std::string (*val.Get<ImageId>());
                IsoBootEmu emu = bootEmus.Resolve (name);
                if(emu == IsoBootEmu::Max)
                {
                    return Image::InvalidId (Image::GetPropName (ImgProp::BootEmu), 
                                             comp.GetOwner().GetName(), name);
                }
                isoComp.bootEmu = emu;
                return Success();
            },
            [] (Component& comp) -> std::optional<std::any>
            {
                IsoPartComp& isoComp = derived<IsoPartComp&>(comp);
                if(isoComp.bootEmu == IsoBootEmu::Max)
                    return std::nullopt;
                return isoComp.bootEmu;
            }
        }
    },
    {ImgProp::BootImage,
        {typeid(ImageId),
            "",
            [] (Component& comp, const ImageVal& val) -> ImageResult
            {
                IsoPartComp& isoComp = derived<IsoPartComp&> (comp);
                isoComp.bootImageName = *val.Get<std::string>();
                return Success();
            },
            [] (Component& comp) -> std::optional<std::any>
            {
                IsoPartComp& isoComp = derived<IsoPartComp&> (comp);
                if(isoComp.bootImageName.empty())
                    return std::nullopt;
                return isoComp.bootImageName;
            }
        }
    }
};

const NameRegistry<IsoBootEmu> IsoPartComp::bootEmus = {
    {"noemu", IsoBootEmu::NoEmu},
    {"hdd",   IsoBootEmu::Hdd},
    {"fdd",   IsoBootEmu::Fdd}
};

const CompConfRegistry FloppyPartComp::registry = {

};
