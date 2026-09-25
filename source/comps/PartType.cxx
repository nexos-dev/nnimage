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

ResNone PartTypeComp::Validate()
{
    return Success();
}

ResNone IsoPartComp::Validate()
{
    assert (bootEmu != IsoBootEmu::Max);
    // If use boot emulation other than noemu, check that an image was provided and resolved
    if (bootEmu != IsoBootEmu::NoEmu)
    {
        if (bootImage == nullptr)
            return ImageError::Invalid ("ISO9660 image missing boot image");

        // Ensure boot image type is valid
        auto res = bootImage->GetComponent<PartTypeComp> (CompType::PartType);
        if (!res)
        {
            if (!owner.GetName().empty())
            {
                res.Error().Add (
                    ImageError::Invalid (std::format ("Failed to query boot image for image \"{}\"", owner.GetName())));
            }
            return res.Error();
        }
        PartTypeComp* bootImgPart = res.Value();
        PartType type = bootImgPart->GetPartType();

        auto& validTypes = validBootImage[bootEmu];
        auto it = std::find (validTypes.begin(), validTypes.end(), type);
        if (it == validTypes.end())
        {
            // TODO better diagnostic here
            return ImageError::Invalid (
                std::format ("Partition layout \"{}\" is invalid for ISO9660 image given specified boot emulation",
                    getTypeName (type)));
        }
    }
    else
    {

        // Noemu can't accept a boot image
        if (bootImage != nullptr)
            return ImageError::Invalid ("ISO9660 boot image not valid for emulation \"noemu\"");
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
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId ("noemu"),
            [] (Component& comp, const ImageVal& val) -> ResNone
            {
                IsoPartComp& isoComp = derived<IsoPartComp> (comp);
                // Resolve the ID
                std::string name = std::move((*val.Get<ImageId>()).Str());
                IsoBootEmu emu = bootEmus.Resolve (name);
                if(emu == IsoBootEmu::Max)
                {
                    return Image::InvalidId (Image::GetPropName (ImgProp::BootEmu), 
                                             comp.GetOwner().GetName(), name);
                }
                isoComp.bootEmu = emu;
                return Success();
            },
            [] (const Component& comp) -> std::optional<std::any>
            {
                const IsoPartComp& isoComp = derived<IsoPartComp> (comp);
                if(isoComp.bootEmu == IsoBootEmu::Max)
                    return std::nullopt;
                return isoComp.bootEmu;
            }
        }
    },
    {ImgProp::BootImage,
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId(""),
            [] (Component& comp, const ImageVal& val) -> ResNone
            {
                IsoPartComp* isoComp = &derived<IsoPartComp&> (comp);
                auto bootImageName = (*val.Get<ImageId>()).Str();
                isoComp->owner.AddImageRef (bootImageName, [isoComp] (Image* image) { isoComp->bootImage = image; });
                return Success();
            },
            [] (const Component& comp) -> std::optional<std::any>
            {
                const IsoPartComp& isoComp = derived<IsoPartComp> (comp);
                if(isoComp.bootImage == nullptr)
                    return std::nullopt;
                return isoComp.bootImage;
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
