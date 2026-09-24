/*
    Image.h - image and config declarations
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

#ifndef IMAGE_H
#define IMAGE_H

#include "include/ConfParser.h"
#include "include/EnumArray.h"
#include "include/image/ImgProp.h"
#include "include/image/ImgBase.h"
#include "include/image/ImgError.h"
#include "include/image/ImgComponent.h"
#include "BackendTypes.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Image;
class Partition;

using ImgConfItem = ConfItem<Image>;
using PartConfItem = ConfItem<Partition>;

using ImgConfRegistry = ConfRegistry<ImgProp, Image>;
using PartConfRegistry = ConfRegistry<PartProp, Partition>;

// NOTE: this struct is in NO WAY copyable and ALWAYS is const
struct PartSpec
{
    std::string name{};
    std::string format{};
    std::string prefix{};
    uint64_t start = PartSpec::Default;
    uint64_t size = PartSpec::Default;
    std::optional<bool> isBoot = std::nullopt;
    static constexpr uint64_t Default = -1;

    // Delete all copy constructors assignment operators
    PartSpec (const PartSpec&) = delete;
    PartSpec& operator= (const PartSpec&) = delete;
    PartSpec() = default;
    ~PartSpec() = default;
};

class Partition : public RegElement<Partition, PartProp, PartConfRegistry>
{
  public:
    Partition (std::string name) : RegElement{ErrorCode::InvalidPartProp, ErrorCode::PartMissingProp}
    {
        spec.name = std::move (name);
    }
    Partition (const Partition&) = delete;
    Partition& operator= (const Partition&) = delete;

    const std::string& GetName()
    {
        return spec.name;
    }

    ResNone Set (std::string_view name, const ImageVal& val);

    template <typename T>
    Result<std::optional<T>> Get (std::string_view name);

    template <typename T>
    Result<std::optional<T>> Get (PartProp prop);

    Result<bool> IsSet (std::string_view name);

    using RegElement<Partition, PartProp, PartConfRegistry>::IsSet;
    using RegElement<Partition, PartProp, PartConfRegistry>::Set;
    using RegElement<Partition, PartProp, PartConfRegistry>::SetDefaults;

    static PartProp ResolveName (std::string_view name)
    {
        return nameRegistry.Resolve (name);
    }
    static const std::string& GetPropName (PartProp prop)
    {
        return nameRegistry.GetName (prop);
    }

    const PartSpec& GetSpec() const
    {
        return spec;
    }

  private:
    // Resolves name to a PartProp and dispatches func(prop)
    template <typename Func>
    auto dispatchByName (std::string_view name, std::string_view partName, Func&& func)
        -> decltype (func (PartProp::Max))
    {
        PartProp prop = ResolveName (name);
        if (prop == PartProp::Max)
            return ImageError::Make (ErrorCode::InvalidPartProp,
                {{"prop", std::string (name)}, {"name_suffix", ImageError::NameSuffix (partName)}});
        return func (prop);
    }

    const PartConfRegistry& getRegistry() override
    {
        return registry;
    }
    std::string_view getRegElementName() const override
    {
        return spec.name;
    }
    std::string_view getPropName (PartProp prop) const override
    {
        return GetPropName (prop);
    }

    PartSpec spec;
    const static PartConfRegistry registry;
    const static NameRegistry<PartProp> nameRegistry;
};

// Base property enums
enum class BootMode
{
    None,
    Bios,
    Efi,
    Max
};

// NOTE NOTE NOTE: this struct is in NO WAY copyable or movable and ALWAYS is const
// If any code ever tries to copy or move it, many things would fail
// It is STRICTLY NON COPYABLE
// Do not attempt to do so. If you do, gcc/clang will find your home and make your life miserable
struct ImgSpec
{
    std::string name{};
    std::string fileExt{};
    uint64_t size = -1;
    BootMode bootMode = BootMode::Max;

    static constexpr uint64_t EmptySize = -1;

    ImgSpec (const ImgSpec&) = delete;
    ImgSpec& operator= (const ImgSpec&) = delete;
    ImgSpec() = default;
    ~ImgSpec() = default;
};

class Image : public RegElement<Image, ImgProp, ImgConfRegistry>
{
  public:
    Image (std::string name) : RegElement{ErrorCode::InvalidImgProp, ErrorCode::ImgMissingProp}
    {
        spec.name = std::move (name);
    }
    const std::string& GetName() const
    {
        return spec.name;
    }

    void SetName (std::string name)
    {
        spec.name = std::move (name);
    }

    bool SetBackend (BackendType backend)
    {
        if (this->backend != BackendType::None)
            return false;
        this->backend = backend;
        return true;
    }
    // Used by backends to identify the image, e.g. the block device name
    void SetBackendTag (std::string tag)
    {
        backendTag = std::move (tag);
    }
    BackendType GetBackendType (BackendType suggestion) const;

    ResNone AddComponent (std::unique_ptr<Component> comp);
    template <class T>
    Result<T*> GetComponent (CompType type);
    bool CheckComponent (CompType type);

    // Set accepts parser-shaped values, Get returns the property's translated value.
    ResNone Set (std::string_view name, const ImageVal& val);
    ResNone Set (ImgProp prop, const ImageVal& val) override;

    template <typename T>
    Result<std::optional<T>> Get (std::string_view name);
    template <typename T>
    Result<std::optional<T>> Get (ImgProp prop);

    Result<bool> IsSet (std::string_view name);
    Result<bool> IsSet (ImgProp prop) override;

    ResNone SetDefaults() override;

    void AddPartition (std::shared_ptr<Partition> part)
    {
        parts.push_back (std::move (part));
    }
    const std::vector<std::shared_ptr<Partition>>& GetPartitions() const
    {
        return parts;
    }

    const ImgSpec& GetSpec() const
    {
        return spec;
    }

    ResNone Finalize();

    static ImgProp ResolveProp (std::string_view name)
    {
        return nameRegistry.Resolve (name);
    }
    static const std::string& GetPropName (ImgProp prop)
    {
        return nameRegistry.GetName (prop);
    }

    virtual ~Image() = default;
    // Delete copy constructor and assignment operator
    Image (const Image&) = delete;
    Image& operator= (const Image&) = delete;

    // Error maker helpers
    static Error InvalidId (std::string_view prop, std::string_view name, std::string_view id)
    {
        return ImageError::InvalidId (prop, name, id);
    }

  private:
    ImgSpec spec;
    std::vector<std::shared_ptr<Partition>> parts;
    BackendType backend;
    std::string backendTag{};
    std::string defaultExt = ".img";

    // Component containers
    EnumArray<CompType, std::unique_ptr<Component>, CompType::Max> comps;

    // Deferred properties
    std::vector<std::pair<ImgProp, ImageVal>> deferredProps;

    // Resolves name to an ImgProp and dispatches func(prop)
    template <typename Func>
    static auto dispatchByName (std::string_view name, std::string_view imgName, Func&& func)
        -> decltype (func (ImgProp::Max))
    {
        ImgProp prop = ResolveProp (name);
        if (prop == ImgProp::Max)
            return ImageError::Make (ErrorCode::InvalidImgProp,
                {{"prop", std::string (name)}, {"name_suffix", ImageError::NameSuffix (imgName)}});
        return func (prop);
    }

    // Resolves prop to its owning component, or nullopt if it's owned by the base image itself.
    std::optional<Component*> resolveComponent (ImgProp prop);

    // Replays deferred properties
    ResNone runDeferred();
    ResNone validate();

    Result<std::optional<std::any>> getInternal (ImgProp prop);

    // Getter/setter for setting a property that adds a component
    template <typename CompT>
    ResNone setCompProp (ImgProp prop, const ImageVal& val);

    template <typename CompT>
    CompT* getCompProp (CompType type);

    const ImgConfRegistry& getRegistry() override
    {
        return baseRegistry;
    }
    std::string_view getRegElementName() const override
    {
        return spec.name;
    }
    std::string_view getPropName (ImgProp prop) const override
    {
        return GetPropName (prop);
    }

    const static ImgConfRegistry baseRegistry;
    const static std::unordered_map<ImgProp, CompType> keyMap;
    const static NameRegistry<ImgProp> nameRegistry;

    const static NameRegistry<BootMode> bootModes;
};

#include "include/image/ImageGet.txx"

#endif
