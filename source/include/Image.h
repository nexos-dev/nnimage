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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <filesystem>
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
    explicit Partition (std::string name) : RegElement{ErrorCode::PartMissingProp}
    {
        spec.name = std::move (name);
    }
    Partition (const Partition&) = delete;
    Partition& operator= (const Partition&) = delete;

    const std::string& GetName() const
    {
        return spec.name;
    }

    ResNone Set (std::string_view name, const ImageVal& val);

    template <typename T>
    Result<std::optional<T>> Get (std::string_view name) const;

    template <typename T>
    std::optional<T> Get (PartProp prop) const;

    Result<bool> IsSet (std::string_view name) const;

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
    Error invalidPartProp (std::string_view propName) const
    {
        return ImageError::Make (ErrorCode::InvalidPartProp,
            {{"prop", std::string (propName)}, {"name_suffix", ImageError::NameSuffix (*this)}});
    }

    const PartConfRegistry& getRegistry() const override
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
    uint64_t size = -1;
    BootMode bootMode = BootMode::Max;
    std::filesystem::path file{};

    static constexpr uint64_t EmptySize = -1;

    ImgSpec (const ImgSpec&) = delete;
    ImgSpec& operator= (const ImgSpec&) = delete;
    ImgSpec() = default;
    ~ImgSpec() = default;
};

struct ImageRef
{
    GenericRef<Image> ref;
    std::function<void (Image*)> setter;
};

template <typename T, typename Self>
using ComponentPtr = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>, const T*, T*>;

class Image : public RegElement<Image, ImgProp, ImgConfRegistry>
{
  public:
    explicit Image (std::string name) : RegElement{ErrorCode::ImgMissingProp}
    {
        spec.name = std::move (name);
    }
    const std::string& GetName() const
    {
        return spec.name;
    }

    void SetFilePath (std::filesystem::path file)
    {
        spec.file = std::move (file);
    }
    const std::filesystem::path& GetFilePath() const
    {
        return spec.file;
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
    BackendType GetBackendType()
    {
        return backend;
    }

    ResNone AddComponent (std::unique_ptr<Component> comp);
    template <typename T>
    auto GetComponent (this auto& self, CompType type) -> ComponentPtr<T, decltype (self)>;
    bool CheckComponent (CompType type) const;

    // Set accepts parser-shaped values, Get returns the property's translated value.
    ResNone Set (std::string_view name, const ImageVal& val);
    ResNone Set (ImgProp prop, const ImageVal& val) override;

    template <typename T>
    Result<std::optional<T>> Get (std::string_view name) const;
    template <typename T>
    std::optional<T> Get (ImgProp prop) const;

    Result<bool> IsSet (std::string_view name) const;
    bool IsSet (ImgProp prop) const override;

    // Adds a name-based reference to an image that will be resolved later
    void AddImageRef (std::string imageName, std::function<void (Image*)> setCb);
    // Gets all references
    const std::vector<ImageRef>& GetRefs() const
    {
        return imageRefs;
    }

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

    // Replays any properties that were deferred because their owning component didn't exist yet
    ResNone ResolveDeferred();

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
    // Basic image data
    ImgSpec spec;
    std::vector<std::shared_ptr<Partition>> parts;
    BackendType backend;
    std::string backendTag{};

    // Component containers
    EnumArray<CompType, std::unique_ptr<Component>, CompType::Max> comps;

    // Deferred properties
    std::vector<std::pair<ImgProp, ImageVal>> deferredProps;

    // References to other images
    std::vector<ImageRef> imageRefs;

    // Resolves prop to its loaded owning component, or nullptr when none is loaded.
    auto resolveComponent (this auto& self, ImgProp prop) -> ComponentPtr<Component, decltype (self)>;

    auto getComponent (this auto& self, CompType type) -> ComponentPtr<Component, decltype (self)>
    {
        if (type == CompType::Max)
            throw ErrorException (ImageError::Make (ErrorCode::UnexpectedComponentType, {}));
        return self.comps[type].get();
    }

    std::optional<std::any> getInternal (ImgProp prop) const;

    // Getter/setter for setting a property that adds a component
    template <typename CompT>
    ResNone setCompProp (ImgProp prop, const ImageVal& val);

    template <typename CompT>
    const CompT* getCompProp (CompType type) const;

    Error invalidImgProp (std::string_view propName) const
    {
        return ImageError::Make (ErrorCode::InvalidImgProp,
            {{"prop", std::string (propName)}, {{"name_suffix"}, ImageError::NameSuffix (*this)}});
    }

    const ImgConfRegistry& getRegistry() const override
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

#include "include/image/ImageTmpl.txx"

#endif
