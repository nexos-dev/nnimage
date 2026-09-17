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
    Partition (const std::string& name) : RegElement{ErrorCode::InvalidPartProp, ErrorCode::PartMissingProp}
    {
        spec.name = name;
    }
    Partition (const Partition&) = delete;
    Partition& operator= (const Partition&) = delete;

    const std::string& GetName()
    {
        return spec.name;
    }

    ImageResult Set (std::string_view name, const ImageVal& val);

    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (const std::string& name);

    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (PartProp prop);

    ResCustom<bool, ImageError> IsSet (std::string_view name);

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
    auto dispatchByName (std::string_view name, const std::string& partName, Func&& func)
        -> decltype (func (PartProp::Max))
    {
        PartProp prop = ResolveName (name);
        if (prop == PartProp::Max)
            return ImageError (ErrorCode::InvalidPartProp, {{"prop", std::string (name)}, {"name", partName}});
        return func (prop);
    }

    const PartConfRegistry& getRegistry() override
    {
        return registry;
    }
    const std::string& getRegElementName() const override
    {
        return spec.name;
    }
    const std::string& getPropName (PartProp prop) const override
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
    Image (const std::string& name) : RegElement{ErrorCode::InvalidImgProp, ErrorCode::ImgMissingProp}
    {
        spec.name = name;
    }
    const std::string& GetName() const
    {
        return spec.name;
    }

    void SetName (const std::string& name)
    {
        spec.name = name;
    }

    bool SetBackend (BackendType backend)
    {
        if (this->backend != BackendType::None)
            return false;
        this->backend = backend;
        return true;
    }
    // Used by backends to identify the image, e.g. the block device name
    void SetBackendTag (const std::string& tag)
    {
        backendTag = tag;
    }
    BackendType GetBackendType (BackendType suggestion) const;

    ImageResult AddComponent (std::unique_ptr<Component> comp);
    template <class T>
    ResCustom<T*, ImageError> GetComponent (CompType type);
    bool CheckComponent (CompType type);

    // Set accepts parser-shaped values, Get returns the property's translated value.
    ImageResult Set (std::string_view name, const ImageVal& val);
    ImageResult Set (ImgProp prop, const ImageVal& val);

    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (std::string_view name);
    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (ImgProp prop);

    ResCustom<bool, ImageError> IsSet (std::string_view name);
    ResCustom<bool, ImageError> IsSet (ImgProp prop);

    ImageResult SetDefaults();

    void AddPartition (std::unique_ptr<Partition> part)
    {
        parts.push_back (std::move (part));
    }
    const std::vector<std::unique_ptr<Partition>>& GetPartitions() const
    {
        return parts;
    }

    const ImgSpec& GetSpec() const
    {
        return spec;
    }

    ImageResult Finalize();

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
    static ImageError InvalidId (const std::string& prop, const std::string& name, const std::string& id)
    {
        return ImageError (ErrorCode::InvalidId, {{"prop", prop}, {"name", name}, {"id", id}});
    }

  private:
    ImgSpec spec;
    std::vector<std::unique_ptr<Partition>> parts;
    BackendType backend;
    std::string backendTag{};
    std::string defaultExt = ".img";

    // Component containers
    EnumArray<CompType, std::unique_ptr<Component>, CompType::Max> comps;

    // Deferred properties
    std::vector<std::pair<ImgProp, ImageVal>> deferredProps;

    // Resolves name to an ImgProp and dispatches func(prop)
    template <typename Func>
    static auto dispatchByName (std::string_view name, const std::string& imgName, Func&& func)
        -> decltype (func (ImgProp::Max))
    {
        ImgProp prop = ResolveProp (name);
        if (prop == ImgProp::Max)
            return ImageError (ErrorCode::InvalidImgProp, {{"prop", std::string (name)}, {"name", imgName}});
        return func (prop);
    }

    // Resolves prop to its owning component, or nullopt if it's owned by the base image itself.
    std::optional<Component*> resolveComponent (ImgProp prop);

    // Replays deferred properties
    ImageResult runDeferred();
    ImageResult validate();

    ResCustom<std::optional<std::any>, ImageError> getInternal (ImgProp prop);

    // Getter/setter for setting a property that adds a component
    template <typename CompT>
    ImageResult setCompProp (ImgProp prop, const ImageVal& val);

    template <typename CompT>
    CompT* getCompProp (CompType type);

    const ImgConfRegistry& getRegistry() override
    {
        return baseRegistry;
    }
    const std::string& getRegElementName() const override
    {
        return spec.name;
    }
    const std::string& getPropName (ImgProp prop) const override
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
