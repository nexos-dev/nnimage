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
ResNone RegElement<Element, Property, Registry>::Set (Property prop, const ImageVal& val)
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
Result<std::optional<std::any>> RegElement<Element, Property, Registry>::Get (Property prop) const
{
    const auto& registry = getRegistry();
    auto it = registry.find (prop);
    if (it == registry.end())
        return makeRegElementError (invalidPropertyCode, getRegElementName(), getPropName (prop));

    return it->second.getter (element());
}

template <typename Element, typename Property, typename Registry>
Result<bool> RegElement<Element, Property, Registry>::IsSet (Property prop) const
{
    auto value = Get (prop);
    if (!value)
        return value.Error();
    return value.Value().has_value();
}

template <typename Element, typename Property, typename Registry>
ResNone RegElement<Element, Property, Registry>::SetDefaults()
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

ResNone Image::AddComponent (std::unique_ptr<Component> comp)
{
    assert (comp);

    CompType type = comp->GetType();
    assert (type != CompType::Max);

    if (comps[type])
        return ImageError::Make (ErrorCode::ComponentOverwrite, {});

    comps[type] = std::move (comp);
    return Success();
}

bool Image::CheckComponent (CompType type) const
{
    return type != CompType::Max && comps[type] != nullptr;
}

ResNone Image::Set (std::string_view name, const ImageVal& val)
{
    return dispatchByName (name, spec.name, [&] (ImgProp prop) { return Set (prop, val); });
}

ResNone Image::Set (ImgProp prop, const ImageVal& val)
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

Result<bool> Image::IsSet (std::string_view name) const
{
    return dispatchByName (name, spec.name, [&] (ImgProp prop) { return IsSet (prop); });
}

Result<bool> Image::IsSet (ImgProp prop) const
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

ResNone Image::SetDefaults()
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

ResNone Image::Finalize()
{
    // Handle all deferred properties now
    auto defRes = ResolveDeferred();
    if (!defRes)
        return defRes;

    // Any property that still couldn't be resolved (e.g. it references a component that never got
    // created) is a hard error
    if (!deferredProps.empty())
    {
        return ImageError::Make (ErrorCode::UnresolvedDeferredProp,
            {{"prop", GetPropName (deferredProps.front().first)}, {"name_suffix", ImageError::NameSuffix (spec.name)}});
    }

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

void Image::AddImageRef (std::string imageName, std::function<void (Image*)> setCb)
{
    imageRefs.push_back ({GenericRef<Image> (std::move (imageName), *this), std::move (setCb)});
}

Result<std::optional<std::any>> Image::getInternal (ImgProp prop) const
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
ResNone Image::setCompProp (ImgProp prop, const ImageVal& val)
{
    std::string id = std::string (*val.Get<ImageId>());
    auto comp = CompT::Factory (id, *this);
    if (!comp)
        return InvalidId (GetPropName (prop), GetName(), id);

    return AddComponent (std::move (comp));
}

template <typename CompT>
const CompT* Image::getCompProp (CompType type) const
{
    if (!CheckComponent (type))
        return nullptr;

    auto res = GetComponent<CompT> (type);
    if (!res)
        return nullptr;

    return res.Value();
}

ResNone Image::ResolveDeferred()
{
    std::vector<std::pair<ImgProp, ImageVal>> unresolved;

    for (auto& prop : deferredProps)
    {
        auto comp = resolveComponent (prop.first);
        auto res =
            comp.has_value() ? (*comp)->Set (prop.first, prop.second) : RegElement::Set (prop.first, prop.second);
        if (!res)
            unresolved.push_back (std::move (prop));
    }

    deferredProps = std::move (unresolved);
    return Success();
}

// Partition functions
ResNone Partition::Set (std::string_view name, const ImageVal& val)
{
    return dispatchByName (name, spec.name, [&] (PartProp prop) { return Set (prop, val); });
}

Result<bool> Partition::IsSet (std::string_view name) const
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

const CompConfRegistry& Component::getRegistry() const
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
                throw ErrorException (ImageError::Make (ErrorCode::PropConflict, {}));

            mergedRegistry.insert (conf);
        }
    }
    return mergedRegistry;
}

// ImageVal stuff

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

Result<ImageVal> ImageVal::FromToken (LexToken tok)
{
    return std::visit (overloaded{[&] (const std::string& x) -> Result<ImageVal> {
                                      if (tok.type == TokenType::Identifier)
                                          return ImageVal (ImageId (std::move (x)), tok.line);
                                      else
                                          return ImageVal (std::move (x), tok.line);
                                  },
                           [&] (uint64_t x) -> Result<ImageVal> { return ImageVal (x, tok.line); },
                           [&] (bool x) -> Result<ImageVal> { return ImageVal (x, tok.line); },
                           [&] (const LexNumId& x) -> Result<ImageVal> {
                               ImageNumId numId = ImageNumId (x.num, x.id);
                               auto res = numId.Parse();
                               if (!res)
                                   return res.Error();
                               return ImageVal (numId);
                           }},
        tok.val);
}

ImageVal ImageVal::Cast (ImageValIdx wantedType) const
{
    // Currently, the only valid cast is from ID->std::string and ID->ImageList
    return std::visit (overloaded{[&] (const ImageId& x) -> ImageVal {
                                      if (wantedType == ImageVal::GetTypeIndex<std::string>())
                                          return ImageVal (std::string (x), line);
                                      else if (wantedType == ImageVal::GetTypeIndex<ImageList>())
                                          return ImageVal (ImageList{x}, line);
                                      return ImageVal::Invalid;
                                  },
                           [&] (auto&&) -> ImageVal { return ImageVal::Invalid; }},
        val);
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

const std::unordered_map<std::string_view, size_t> ImageNumId::mulMap = {
    {"B", 1},
    {"KiB", 1024},
    {"KB", 1000},
    {"MiB", 1024 * 1024},
    {"MB", 1000 * 1000},
    {"GiB", 1024 * 1024 * 1024},
    {"GB", 1000 * 1000 * 1000},
    {"TiB", static_cast<size_t> (1024) * 1024 * 1024 * 1024},
    {"TB", static_cast<size_t> (1000) * 1000 * 1000 * 1000}
};

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
            [] (Image& img, const ImageVal& val) -> ResNone
            {
                img.spec.size = (*val.Get<ImageNumId>()).Get();
                return Success();
            },
            [] (const Image& img) -> std::optional<std::any>
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
            [] (Image& img, const ImageVal& val) -> ResNone
            {
                std::string id = std::move((*val.Get<ImageId>()).Str());
                BootMode mode = bootModes.Resolve (id);
                if(mode == BootMode::Max)
                    return InvalidId (GetPropName (ImgProp::BootMode), img.spec.name, id);

                img.spec.bootMode = mode;
                return Success();
            },
            [] (const Image& img) -> std::optional<std::any>
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
            [] (Image& img, const ImageVal& val) -> ResNone
            {
                return img.setCompProp<PartTypeComp> (ImgProp::PartType, val);
            },
            [] (const Image& img) -> std::optional<std::any>
            {
                auto comp = img.getCompProp<PartTypeComp> (CompType::PartType);
                if (!comp)
                    return std::nullopt;

                return comp->GetPartType();
            }
        }
    },
    {ImgProp::Format,
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId ("raw"),
            [] (Image& img, const ImageVal& val) -> ResNone
            {
                return img.setCompProp<FormatComp> (ImgProp::Format, val);
            },
            [] (const Image& img) -> std::optional<std::any>
            {
                auto comp = img.getCompProp<FormatComp> (CompType::Format);
                if (!comp)
                    return std::nullopt;

                return comp->GetFormatType();
            }
        }
    },
    {ImgProp::BootLoad,
        {ImageVal::GetTypeIndex<ImageId>(),
            ImageId ("none"),
            [] (Image& img, const ImageVal& val) -> ResNone
            {
                return img.setCompProp<BootLoadComp> (ImgProp::BootLoad, val);
            },
            [] (const Image& img) -> std::optional<std::any>
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
            [] (Partition& part, const ImageVal& val) -> ResNone
            {
                part.spec.start = (*val.Get<ImageNumId>()).Get();
                return Success();
            },
            [] (const Partition& part) -> std::optional<std::any>
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
            [] (Partition& part, const ImageVal& val) -> ResNone
            {
                part.spec.size = (*val.Get<ImageNumId>()).Get();
                return Success();
            },
            [] (const Partition& part) -> std::optional<std::any>
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
            [] (Partition& part, const ImageVal& val) -> ResNone
            {
                part.spec.format = std::move((*val.Get<ImageId>()).Str());
                return Success();
            },
            [] (const Partition& part) -> std::optional<std::any>
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
            [] (Partition& part, const ImageVal& val) -> ResNone
            {
                part.spec.prefix = *val.Get<std::string>();
                return Success();
            },
            [] (const Partition& part) -> std::optional<std::any>
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
            [] (Partition& part, const ImageVal& val) -> ResNone
            {
                part.spec.isBoot = *val.Get<bool>();
                return Success();
            },
            [] (const Partition& part) -> std::optional<std::any>
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
