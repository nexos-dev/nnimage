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

#include "include/Image.h"

#include <cassert>
#include <sstream>

BackendType Image::GetBackendType (BackendType suggestion) const
{
    return BackendType::None;
}

ImageResult Image::AddComponent (std::unique_ptr<Component> comp)
{
    assert (comp);

    CompType type = comp->GetType();
    assert (type != CompType::Max);

    if (comps[type])
        return ImageError (ErrorCode::ComponentOverwrite, "Can't overwrite component");

    comps[type] = std::move (comp);
    return Success();
}

bool Image::CheckComponent (CompType type)
{
    return type != CompType::Max && comps[type] != nullptr;
}

ImageResult Image::Set (std::string_view name, const ImageVal& val)
{
    return dispatchByName (name, spec.name, [&] (ImgProp prop) { return Set (prop, val); });
}

ImageResult Image::Set (ImgProp prop, const ImageVal& val)
{
    // First try component
    auto compRes = resolveComponent (prop);
    if (!compRes.IsOk())
        return compRes.GetError();

    auto comp = compRes.GetValue();
    if (comp.has_value())
        return (*comp)->Set (prop, val);

    return setBase (prop, val);
}

ResCustom<bool, ImageError> Image::IsSet (std::string_view name)
{
    return dispatchByName (name, spec.name, [&] (ImgProp prop) { return IsSet (prop); });
}

ResCustom<bool, ImageError> Image::IsSet (ImgProp prop)
{
    auto compRes = resolveComponent (prop);
    if (!compRes.IsOk())
        return compRes.GetError();

    auto comp = compRes.GetValue();
    if (comp.has_value())
    {
        auto getRes = (*comp)->Get (prop);
        if (!getRes.IsOk())
            return getRes.GetError();
        return getRes.GetValue().has_value();
    }

    return checkSetBase (prop);
}

ResCustom<std::optional<Component*>, ImageError> Image::resolveComponent (ImgProp prop)
{
    auto it = keyMap.find (prop);
    assert (it != keyMap.end());

    CompType owner = it->second;
    if (owner == CompType::Max)
        return std::optional<Component*>{};

    Component* comp = comps[owner].get();
    if (!comp)
        return ImageError (ErrorCode::InvalidImgProp, {{"prop_name", GetPropName (prop)}, {"name", spec.name}});

    return std::optional<Component*> (comp);
}

ImageResult Image::setBase (ImgProp prop, const ImageVal& val)
{
    // Get the registry entry
    auto it = baseRegistry.find (prop);
    assert (it != baseRegistry.end());

    const auto& conf = it->second;
    if (conf.inputType != val.GetType())
        return ImageError (ErrorCode::PropTypeMismatch, {{"prop_name", GetPropName (prop)}});

    return conf.setter (*this, val);
}

bool Image::checkSetBase (ImgProp prop)
{
    auto it = baseRegistry.find (prop);
    assert (it != baseRegistry.end());

    const auto& conf = it->second;

    return conf.getter (*this).has_value();
}

ImageResult Image::SetDefaults()
{
    // Apply base level defaults
    auto res = applyDefaults (baseRegistry);
    if (!res.IsOk())
        return res.GetError();

    // Now apply for each component
    for (auto it = comps.begin(); it != comps.end(); it++)
    {
        auto* comp = it->get();
        if (comp)
        {
            res = comp->SetDefaults();
            if (!res.IsOk())
                return res;
        }
    }

    return Success();
}

ImageResult Image::applyDefaults (const ImgConfRegistry& registry)
{
    for (const auto& [prop, conf] : registry)
    {
        auto res = applyDefault (conf);
        if (!res.IsOk())
        {
            // Only MissingProp error is valid here; all others are programing errors
            ImageError& err = res.GetError();
            assert (err.LastFrame().code == ErrorCode::ImgMissingProp);
            err.AddKey ({{"prop", GetPropName (prop)}});
            return err;
        }
    }
    return Success();
}

ImageResult Image::applyDefault (const ImgConfItem& conf)
{
    // Don't overwrite a set value
    if (conf.getter (*this).has_value())
        return Success();

    // Is has no default, error out
    // TODO: should this really be an error?
    else if (std::holds_alternative<std::monostate> (conf.defaultVal))
        return ImageError (ErrorCode::ImgMissingProp, {{"prop", ""}, {"name", spec.name}});

    return conf.setter (*this, conf.defaultVal);
}

ImageResult Image::Validate()
{
    return Success();
}

ImageResult Partition::Set (std::string_view name, const ImageVal& val)
{
    return dispatchByName (name, spec.name, [&] (PartProp prop) { return Set (prop, val); });
}

ImageResult Partition::Set (PartProp prop, const ImageVal& val)
{
    auto entryRes = resolveEntry (prop);
    if (!entryRes.IsOk())
        return entryRes.GetError();

    auto conf = entryRes.GetValue();
    if (conf.inputType != val.GetType())
        return ImageError (ErrorCode::PropTypeMismatch, {{"prop_name", GetPropName (prop)}, {"name", spec.name}});

    return conf.setter (*this, val);
}

ResCustom<bool, ImageError> Partition::IsSet (std::string_view name)
{
    return dispatchByName (name, spec.name, [&] (PartProp prop) { return IsSet (prop); });
}

ResCustom<bool, ImageError> Partition::IsSet (PartProp prop)
{
    auto entryRes = resolveEntry (prop);
    if (!entryRes.IsOk())
        return entryRes.GetError();

    const auto& conf = entryRes.GetValue();
    return conf.getter (*this).has_value();
}

ResCustom<PartConfItem, ImageError> Partition::resolveEntry (PartProp prop)
{
    auto it = registry.find (prop);
    if (it == registry.end())
        return ImageError (ErrorCode::InvalidPartProp, {{"prop_name", GetPropName (prop)}, {"name", spec.name}});

    return it->second;
}

ImageResult Partition::SetDefaults()
{
    for (const auto& [prop, conf] : registry)
    {
        auto res = applyDefault (conf);
        if (!res.IsOk())
        {
            ImageError& err = res.GetError();
            // This is the only error that can occur validly; other errors are programing errors
            assert (err.LastFrame().code == ErrorCode::PartMissingProp);
            err.AddKey ({{"prop", GetPropName (prop)}});
            return err;
        }
    }
    return Success();
}

ImageResult Partition::applyDefault (const PartConfItem& conf)
{
    // Don't overwrite
    if (conf.getter (*this).has_value())
        return Success();

    // TODO: should this really be an error?
    else if (std::holds_alternative<std::monostate> (conf.defaultVal))
        return ImageError (ErrorCode::PartMissingProp, {{"prop", ""}, {"name", spec.name}});

    return conf.setter (*this, conf.defaultVal);
}

ImageResult Component::Set (ImgProp prop, const ImageVal& val)
{
    return Success();
}

ResCustom<std::optional<std::any>, ImageError> Component::Get (ImgProp prop)
{}

ImageResult Component::SetDefaults()
{
    return Success();
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
            assertKeys ({"type"});
            msg << "Invalid image type \"" << getString ("type") << "\" specified on image" << getName();
            break;
        case ErrorCode::InvalidImgProp:
            assertKeys ({"prop_name"});
            msg << "Unrecognized property \"" << getString ("prop_name") << "\" specified on image" << getName();
            break;
        case ErrorCode::BadFloppySize:
            msg << "Floppy disc" << getName() << " must have size 720K, 1.44M, or 2.88M";
            break;
        case ErrorCode::InvalidPartProp:
            assertKeys ({"prop_name"});
            msg << "Unrecognized property \"" << getString ("prop_name") << "\" specified on partition" << getName();
            break;
        case ErrorCode::PropTypeMismatch:
            assertKeys ({"prop_name"});
            msg << "Invalid type specified on property \"" << getString ("prop_name") << "\"";
            break;
        case ErrorCode::InvalidId:
            assertKeys ({"id", "prop_name"});
            msg << "Invalid ID \"" << getString ("id") << "\" specified for property \"" << getString ("prop_name")
                << "\" on image" << getName();
            break;
        case ErrorCode::ImgMissingProp:
            assertKeys ({"prop"});
            msg << "Missing required property \"" << getString ("prop") << "\" on image " << getName();
            break;
        case ErrorCode::PartMissingProp:
            assertKeys ({"prop"});
            msg << "Missing required property \"" << getString ("prop") << "\" on partition " << getName();
            break;
        default:
            msg << frame.msg;
    }
    frame.msg = msg.str();
}

// Now begins the all-important registries
// clang-format off

// NOTE: this contains names across for all components. It might be a questionable design choice but it's
// simpler then having to chase down 100 different registries when we already do enough of that here
const NameRegistry<ImgProp> Image::nameRegistry (MakeImgPropRegistry());

// Property-to-owning-component table
const std::unordered_map<ImgProp, CompType> Image::keyMap = [](){
    constexpr auto table = MakeImgPropOwnerTable();
    return std::unordered_map<ImgProp, CompType>{table.begin(), table.end()};
}();

// Keyword tables

const NameRegistry<BootMode> Image::bootModes = {
    {"none", BootMode::None},
    {"bios", BootMode::Bios},
    {"efi", BootMode::Efi},
    {"uefi", BootMode::Efi}
};

// NOTE: when the day comes that C++26 is fully ratified and implemented, the first thing I'm doing is
// erasing this whole thing and using reflection to simplify this mess

const ImgConfRegistry Image::baseRegistry = {
    {ImgProp::Size,
        {typeid (ImageNumId),
            std::monostate{},
            [] (Image& img, const ImageVal& val) -> ImageResult 
            {
                img.spec.size = (*val.Get<ImageNumId>()).Get();
                return Success();
            },
            [] (Image& img) -> std::optional<std::any> 
            {
                if (img.spec.size < 0)
                    return std::nullopt;
                return img.spec.size;
            }
        }
    },
    {ImgProp::BootMode,
        {typeid (BootMode),
            "none",
            [] (Image& img, const ImageVal& val) -> ImageResult
            {
                std::string id = *val.Get<std::string>();
                BootMode mode = bootModes.Resolve (id);
                if(mode == BootMode::Max)
                    return InvalidId (GetPropName (ImgProp::BootMode), img.spec.name, id);
                img.spec.bootMode = mode;
                return Success();
            },
            [] (Image& img) -> std::optional<std::any>
            {
                if(img.spec.bootMode == BootMode::Max)
                    return std::nullopt;
                return img.spec.bootMode;
            }
        }
        
    }
};

// Partition types registry

const NameRegistry<PartProp> Partition::nameRegistry (MakePartPropRegistry());

const PartConfRegistry Partition::registry = {
    {PartProp::Start,
        {typeid (ImageNumId),
            std::monostate{},
            [] (Partition& part, const ImageVal& val) -> ImageResult 
            {
                part.spec.start = (*val.Get<ImageNumId>()).Get();
                return Success();
            },
            [] (Partition& part) -> std::optional<std::any> 
            {
                if (part.spec.start == PartSpec::Default)
                    return std::nullopt;
                return part.spec.start;
            }
        }
    },
    {PartProp::Size,
        {typeid (ImageNumId),
            std::monostate{},
            [] (Partition& part, const ImageVal& val) -> ImageResult 
            {
                part.spec.size = (*val.Get<ImageNumId>()).Get();
                return Success();
            },
            [] (Partition& part) -> std::optional<std::any> 
            {
                if (part.spec.size == PartSpec::Default)
                    return std::nullopt;
                return part.spec.size;
            }
        }
    },
    {PartProp::Format,
        {typeid (std::string),
            "",
            [] (Partition& part, const ImageVal& val) -> ImageResult 
            {
                part.spec.format = *val.Get<std::string>();
                return Success();
            },
            [] (Partition& part) -> std::optional<std::any> 
            {
                if (part.spec.format.empty())
                    return std::nullopt;
                return part.spec.format;
            }
        }
    },
    {PartProp::Prefix,
        {typeid (std::string),
            "",
            [] (Partition& part, const ImageVal& val) -> ImageResult 
            {
                part.spec.prefix = *val.Get<std::string>();
                return Success();
            },
            [] (Partition& part) -> std::optional<std::any> 
            {
                if (part.spec.prefix.empty())
                    return std::nullopt;
                return part.spec.prefix;
            }
        }
    },
    {PartProp::IsBoot,
        {typeid (bool),
            false,
            [] (Partition& part, const ImageVal& val) -> ImageResult 
            {
                part.spec.isBoot = *val.Get<bool>();
                return Success();
            },
            [] (Partition& part) -> std::optional<std::any> 
            {
                if (!part.spec.isBoot.has_value())
                    return std::nullopt;
                return *part.spec.isBoot;
            }
        }
    }
};

#include "CompTable.h"
