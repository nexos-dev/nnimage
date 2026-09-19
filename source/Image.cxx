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
#include "include/SimpleLexer.h"

#include <cassert>
#include <sstream>

// Generic registry class functions
// Pardon all the template noise

template <typename Element, typename Property, typename Registry>
ImageResult RegElement<Element, Property, Registry>::Set (Property prop, const ImageVal& val)
{
    const auto& registry = getRegistry();
    auto it = registry.find (prop);
    if (it == registry.end())
        return makeRegElementError (invalidPropertyCode, getRegElementName(), getPropName (prop));

    const auto& conf = it->second;

    if (conf.inputType != val.GetType())
    {
        // First try to cast it
        ImageVal casted = val.Cast (conf.inputType);
        if (!casted.IsInvalid())
            return conf.setter (element(), casted);
        else
            return makeRegElementError (ErrorCode::PropTypeMismatch, getRegElementName(), getPropName (prop));
    }

    return conf.setter (element(), val);
}

template <typename Element, typename Property, typename Registry>
ResCustom<std::optional<std::any>, ImageError> RegElement<Element, Property, Registry>::Get (Property prop)
{
    const auto& registry = getRegistry();
    auto it = registry.find (prop);
    if (it == registry.end())
        return makeRegElementError (invalidPropertyCode, getRegElementName(), getPropName (prop));

    return it->second.getter (element());
}

template <typename Element, typename Property, typename Registry>
ResCustom<bool, ImageError> RegElement<Element, Property, Registry>::IsSet (Property prop)
{
    auto value = Get (prop);
    if (!value)
        return value.Error();
    return value.Value().has_value();
}

template <typename Element, typename Property, typename Registry>
ImageResult RegElement<Element, Property, Registry>::SetDefaults()
{
    for (const auto& [prop, conf] : getRegistry())
    {
        // Don't overwrite an existing value
        if (conf.getter (element()).has_value())
            continue;

        // Error out, monostate means there is no default and it is required
        // TODO: should we do this?
        if (std::holds_alternative<std::monostate> (conf.defaultVal))
            return makeRegElementError (missingPropertyCode, getRegElementName(), getPropName (prop));

        auto result = conf.setter (element(), ImageVal (conf.defaultVal));
        if (!result)
            return result.Error();
    }
    return Success();
}

// Begin Image class

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
    auto comp = resolveComponent (prop);
    if (comp.has_value())
        return (*comp)->Set (prop, val);

    // CHeck if it's on the base
    if (hasProperty (prop))
        return RegElement::Set (prop, val);

    // Otherwise defer it
    deferredProps.push_back ({prop, val});

    return Success();
}

ResCustom<bool, ImageError> Image::IsSet (std::string_view name)
{
    return dispatchByName (name, spec.name, [&] (ImgProp prop) { return IsSet (prop); });
}

ResCustom<bool, ImageError> Image::IsSet (ImgProp prop)
{
    auto comp = resolveComponent (prop);
    if (comp.has_value())
    {
        auto getRes = (*comp)->Get (prop);
        if (!getRes)
            return getRes.Error();
        return getRes.Value().has_value();
    }

    return RegElement::IsSet (prop);
}

ImageResult Image::SetDefaults()
{
    // Apply base level defaults
    auto res = RegElement::SetDefaults();
    if (!res)
        return res.Error();

    // Now apply for each component
    for (auto it = comps.begin(); it != comps.end(); it++)
    {
        auto* comp = it->get();
        if (comp)
        {
            res = comp->SetDefaults();
            if (!res)
                return res;
        }
    }

    return Success();
}

ImageResult Image::Finalize()
{
    // Handle all deferred properties now
    auto defRes = runDeferred();
    if (!defRes)
        return defRes;

    // Finalize the image
    auto valRes = validate();
    if (!valRes)
        return valRes;

    // Now validate each component
    for (auto it = comps.begin(); it != comps.end(); it++)
    {
        auto* comp = it->get();
        if (comp)
        {
            auto res = comp->Validate();
            if (!res)
                return res;
        }
    }

    return Success();
}

ResCustom<std::optional<std::any>, ImageError> Image::getInternal (ImgProp prop)
{
    std::optional<std::any> val{};
    auto comp = resolveComponent (prop);

    if (comp.has_value())
    {
        assert (*comp);
        auto getRes = (*comp)->Get (prop);

        if (!getRes)
            return getRes.Error();

        val = std::move (getRes.Value());
    }
    else
    {
        auto getRes = RegElement::Get (prop);
        if (!getRes)
            return getRes.Error();

        val = std::move (getRes.Value());
    }

    if (!val.has_value())
        return std::optional<std::any>{};

    return val;
}

template <typename CompT>
ImageResult Image::setCompProp (ImgProp prop, const ImageVal& val)
{
    std::string id = std::string (*val.Get<ImageId>());
    auto comp = CompT::Factory (id, *this);
    if (!comp)
        return InvalidId (GetPropName (prop), GetName(), id);

    return AddComponent (std::move (comp));
}

template <typename CompT>
CompT* Image::getCompProp (CompType type)
{
    if (!CheckComponent (type))
        return nullptr;

    auto res = GetComponent<CompT> (type);
    if (!res)
        return nullptr;

    return res.Value();
}

std::optional<Component*> Image::resolveComponent (ImgProp prop)
{
    auto it = keyMap.find (prop);
    assert (it != keyMap.end());

    CompType owner = it->second;
    if (owner == CompType::Max)
        return std::nullopt;

    Component* comp = comps[owner].get();
    if (!comp)
        return std::nullopt;

    return comp;
}

ImageResult Image::runDeferred()
{
    for (const auto& prop : deferredProps)
    {
        auto comp = resolveComponent (prop.first);
        if (comp.has_value())
            return (*comp)->Set (prop.first, prop.second);

        return RegElement::Set (prop.first, prop.second);
    }
    return Success();
}

ImageResult Image::validate()
{
    // Ensure a partition exists
    if (parts.size() < 1)
        return ImageError (ErrorCode::MissingPart, {{"name", spec.name}});
    return Success();
}

// Partition functions
ImageResult Partition::Set (std::string_view name, const ImageVal& val)
{
    return dispatchByName (name, spec.name, [&] (PartProp prop) { return Set (prop, val); });
}

ResCustom<bool, ImageError> Partition::IsSet (std::string_view name)
{
    return dispatchByName (name, spec.name, [&] (PartProp prop) { return IsSet (prop); });
}

// Component functions
std::string_view Component::getRegElementName() const
{
    return owner.GetName();
}

std::string_view Component::getPropName (ImgProp prop) const
{
    return Image::GetPropName (prop);
}

const CompConfRegistry& Component::getRegistry()
{
    // Check if merging is need
    if (mergedRegistry.empty())
    {
        // Get the two registries
        const auto& mainReg = getMainRegistry();
        const auto& subReg = getSubRegistry();

        // Add every entry into the registry. Start with the main, and then sub. If a conflict occurs, that's an error
        for (const auto& conf : mainReg)
            mergedRegistry.insert (conf);

        for (const auto& conf : subReg)
        {
            // Check if it was already added
            // We throw here as this is a programming error, but I don't really want to assert it
            // in case it did seep through
            if (mergedRegistry.find (conf.first) != mergedRegistry.end())
                throw ErrorException (ImageError (ErrorCode::PropConflict, ""));

            mergedRegistry.insert (conf);
        }
    }
    return mergedRegistry;
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
    {"efi",  BootMode::Efi},
    {"uefi", BootMode::Efi}
};

// NOTE: when the day comes that C++26 is fully ratified and implemented, the first thing I'm doing is
// erasing this whole thing and using reflection to simplify this mess

const ImgConfRegistry Image::baseRegistry = {
    {ImgProp::Size,
        {ImageVal::GetTypeIndex<ImageNumId>(),
            std::monostate{},
            [] (Image& img, const ImageVal& val) -> ImageResult 
            {
                img.spec.size = (*val.Get<ImageNumId>()).Get();
                return Success();
            },
            [] (Image& img) -> std::optional<std::any> 
            {
                if (img.spec.size == ImgSpec::EmptySize)
                    return std::nullopt;
                return img.spec.size;
            }
        }
    },
    {ImgProp::BootMode,
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId ("none"),
            [] (Image& img, const ImageVal& val) -> ImageResult
            {
                std::string id = std::move((*val.Get<ImageId>()).Str());
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
    },
    {ImgProp::PartType,
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId ("gpt"),
            [] (Image& img, const ImageVal& val) -> ImageResult
            {
                return img.setCompProp<PartTypeComp> (ImgProp::PartType, val);
            },
            [] (Image& img) -> std::optional<std::any>
            {
                auto comp = img.getCompProp<PartTypeComp> (CompType::PartType);
                if (!comp)
                    return std::nullopt;

                return comp->GetPartType();
            }
        }
    },
    {ImgProp::BootLoad,
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId ("none"),
            [] (Image& img, const ImageVal& val) -> ImageResult
            {
                return img.setCompProp<BootLoadComp> (ImgProp::BootLoad, val);
            },
            [] (Image& img) -> std::optional<std::any>
            {
                auto comp = img.getCompProp<BootLoadComp> (CompType::Boot);
                if (!comp)
                    return std::nullopt;

                return comp->GetBootType();
            }
        }
    }
};

// Partition types registry

const NameRegistry<PartProp> Partition::nameRegistry (MakePartPropRegistry());

const PartConfRegistry Partition::registry = {
    {PartProp::Start,
        {ImageVal::GetTypeIndex<ImageNumId>(),
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
        {ImageVal::GetTypeIndex<ImageNumId>(),
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
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId (""),
            [] (Partition& part, const ImageVal& val) -> ImageResult 
            {
                part.spec.format = std::move((*val.Get<ImageId>()).Str());
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
        {ImageVal::GetTypeIndex<std::string>(),
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
        {ImageVal::GetTypeIndex<bool>(),
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

template class RegElement<Image, ImgProp, ImgConfRegistry>;
template class RegElement<Partition, PartProp, PartConfRegistry>;
template class RegElement<Component, ImgProp, CompConfRegistry>;

#include "CompTable.h"
