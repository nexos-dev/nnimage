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

#ifndef NNIMAGE_IMAGE_H
#define NNIMAGE_IMAGE_H

#include "include/ConfParser.h"
#include "MemoryMapped.h"
#include "config.h"
#include "BackendTypes.h"

enum class ImgType
{
    Mbr,
    Gpt,
    Iso9660,
    Floppy,
    Max
};

enum class BootMode
{
    None,
    Bios,
    Efi,
    Max
};

enum class ImgProp
{
    Size,
    BootMode,
    BootEmu,
    BootImage,
    MbrFile,
    VbrFile,
    Max
};

enum class PartProp
{
    Start,
    Size,
    Format,
    Prefix,
    IsBoot,
    Max
};

template <typename Value>
class NameRegistry
{
  public:
    NameRegistry (std::initializer_list<std::pair<const std::string, Value>> values) : values{values}
    {}

    Value Resolve (const std::string& name) const
    {
        auto it = values.find (name);
        if (it == values.end())
            return Value::Max;
        return it->second;
    }

    std::string GetName (Value value) const
    {
        auto it = std::find_if (values.begin(), values.end(), [value] (const auto& pair) {
            return pair.second == value;
        });
        assert (it != values.end());
        return it->first;
    }

  private:
    std::unordered_map<std::string, Value> values;
};

// NOTE NOTE NOTE: this struct is in NO WAY copyable or movable and ALWAYS is const
// If any code ever tries to copy or move it, many things would fail
// It is STRICTLY NON COPYABLE and NON MOVABLE
// Do not attempt to do so. If you do, gcc/clang will find your home and make your life miserable
struct ImgSpec
{
    std::string name;
    std::string fileExt = ".img";
    ImgType type = ImgType::Gpt;
    int64_t size = -1;
    BootMode bootMode = BootMode::None;
    ImgSpec (const ImgSpec&) = delete;
    ImgSpec& operator= (const ImgSpec&) = delete;
    ImgSpec (const ImgSpec&&) = delete;
    ImgSpec& operator= (const ImgSpec&&) = delete;
    ImgSpec() = default;
    ~ImgSpec() = default;
};

// NOTE: this struct is in NO WAY copyable or movable and ALWAYS is const
struct PartSpec
{
    std::string name;
    std::string format;
    std::string prefix;
    int64_t start = PartSpec::Default;
    int64_t size = PartSpec::Default;
    bool isBoot = false;
    static constexpr int64_t Default = -1;
    // Delete all copy and move constructors and assignment operators
    PartSpec (const PartSpec&) = delete;
    PartSpec& operator= (const PartSpec&) = delete;
    PartSpec (const PartSpec&&) = delete;
    PartSpec& operator= (const PartSpec&&) = delete;
    PartSpec() = default;
    ~PartSpec() = default;
};

class ImageNumId
{
  public:
    ImageNumId() = default;
    ImageNumId (size_t num, const std::string& mul) : num{num}, mul{mul}
    {}
    bool Parse()
    {
        auto it = mulMap.find (mul);
        if (it == mulMap.end())
            return false;
        if (__builtin_mul_overflow (num, it->second, &val))
            return false;
        valid = true;
        return true;
    }
    int64_t Get() const
    {
        if (valid)
            return val;
        else
            throw std::runtime_error ("Access to uninitialized numid");
    }

  private:
    int64_t val = 0;
    bool valid = false;
    size_t num = 0;
    std::string mul{};
    inline static const std::unordered_map<std::string, size_t> mulMap = {{"B", 1},
        {"KiB", 1024},
        {"KB", 1000},
        {"MiB", 1024 * 1024},
        {"MB", 1000 * 1000},
        {"GiB", 1024 * 1024 * 1024},
        {"GB", 1000 * 1000 * 1000},
        {"TiB", static_cast<size_t> (1024) * 1024 * 1024 * 1024},
        {"TB", static_cast<size_t> (1000) * 1000 * 1000 * 1000}};
};

// HACK: used purely to differentiate between a quoted string and an ID in the variant
struct ImageId
{
    ImageId() = default;
    ImageId (const std::string& str) : id{str}
    {}
    std::string operator()() const
    {
        return id;
    }
    explicit operator std::string() const
    {
        return id;
    }
    std::string& Str()
    {
        return id;
    }

  private:
    std::string id{};
};

using ImageList = std::vector<std::string>;
using ImageValType = std::variant<int64_t, ImageId, std::string, ImageList, bool, ImageNumId, std::monostate>;

class ImageVal
{
  public:
    ImageVal() = default;
    ImageVal (const ImageValType& val, int line = -1) : val{val}, line{line}
    {}
    bool IsEmpty() const
    {
        return std::holds_alternative<std::monostate> (val);
    }
    int GetLine() const
    {
        return line;
    }
    template <typename T>
    std::optional<T> Get() const
    {
        if (!std::holds_alternative<T> (val))
            return {};
        return std::get<T> (val);
    }
    std::type_index GetType() const
    {
        return std::visit ([] (auto&& arg) -> std::type_index { return typeid (arg); }, val);
    }

  private:
    const ImageValType val = std::monostate{};
    int line = -1;
};

using ImageErrProp = std::pair<std::string, std::any>;

class ImageError : public Error
{
  public:
    ImageError() = default;
    ImageError (ErrorCode code, std::initializer_list<ImageErrProp> args)
        : Error ({ErrorDomain::ImageConf, code}, "")
    {
        frame = &frames[frames.size() - 1];
        keys.insert (args.begin(), args.end());
        makeMessage (*frame);
    }
    ImageError (ErrorCode code, std::string msg) : Error ({ErrorDomain::ImageConf, code}, msg)
    {
        frame = &frames[frames.size() - 1];
    }
    ImageError& AddKey (std::initializer_list<ImageErrProp> args)
    {
        keys.insert (args.begin(), args.end());
        makeMessage (*frame);    // Reset the message
        return *this;
    }

  private:
    void makeMessage (ErrorFrame& frame);
    void assertKeys (const std::vector<std::string>& keys)
    {
        for (const auto& key : keys)
            assert (this->keys.find (key) != this->keys.end());
    }
    std::string getString (std::string key)
    {
        return std::any_cast<std::string> (keys[key]);
    }
    std::string getString (std::unordered_map<std::string, std::any>::iterator it)
    {
        return std::any_cast<std::string> (it->second);
    }
    template <typename T>
    T getValue (std::string key)
    {
        return std::any_cast<T> (keys[key]);
    }
    ErrorFrame* frame;
    std::unordered_map<std::string, std::any> keys;
};

using ImageResult = ResCustom<NoResult, ImageError>;

class Image;
class Partition;

using ImgConfSetter = std::function<ImageResult (Image&, const ImageVal&)>;
using PartConfSetter = std::function<ImageResult (Partition&, const ImageVal&)>;
using ImgConfGetter = std::function<std::optional<std::any> (Image&)>;
using PartConfGetter = std::function<std::optional<std::any> (Partition&)>;

struct ImgConfItem
{
    std::type_index inputType;
    ImgConfSetter setter;
    ImgConfGetter getter;
};

struct PartConfItem
{
    std::type_index inputType;
    PartConfSetter setter;
    PartConfGetter getter;
};

using ImgConfRegistry = std::unordered_map<ImgProp, ImgConfItem>;
using PartConfRegistry = std::unordered_map<PartProp, PartConfItem>;

class Partition
{
  public:
    Partition (const std::string& name)
    {
        spec.name = name;
    }
    const std::string& GetName()
    {
        return spec.name;
    }

    ImageResult Set (const std::string& name, const ImageVal& val);
    ImageResult Set (PartProp key, const ImageVal& val);

    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (const std::string& name);
    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (PartProp prop);

    PartProp ResolveName (const std::string& name)
    {
        return nameRegistry.Resolve (name);
    }
    std::string GetPropName (PartProp prop)
    {
        return nameRegistry.GetName (prop);
    }

    const PartSpec& GetSpec() const
    {
        return spec;
    }

  private:
    PartSpec spec;
    const static PartConfRegistry registry;
    const static NameRegistry<PartProp> nameRegistry;
};

class PartRef
{
  public:
    PartRef (const std::string& name, const std::string& image, int line = -1)
        : name{name}, line{line}, image{image}
    {}
    std::string GetName() const
    {
        return name;
    }
    int GetLine() const
    {
        return line;
    }
    std::string GetImage() const
    {
        return image;
    }

  private:
    std::string image;
    std::string name;
    int line;
};

using ImgConstruct = std::function<std::unique_ptr<Image> (const std::string&)>;

class Image
{
  public:
    const std::string& GetName() const
    {
        return spec.name;
    }
    ImgType GetType() const
    {
        return spec.type;
    }

    void SetName (const std::string& name)
    {
        spec.name = name;
    }
    void SetType (ImgType type)
    {
        spec.type = type;
    }

    bool SetBackend (std::shared_ptr<Backend> backend)
    {
        if (this->backend != nullptr)
            return false;
        this->backend = backend;
        return true;
    }
    Backend* GetBackend()
    {
        return backend.get();
    }
    // Used by backends to identify the image, e.g. the block device name
    void SetBackendTag (const std::string& tag)
    {
        backendTag = tag;
    }
    BackendType GetBackendType (BackendType suggestion) const;

    // Set accepts parser-shaped values; Get returns the property's translated domain value.
    ImageResult Set (const std::string& name, const ImageVal& val);
    ImageResult Set (ImgProp prop, const ImageVal& val);

    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (const std::string& name);
    template <typename T>
    ResCustom<std::optional<T>, ImageError> Get (ImgProp prop);

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

    virtual ImageResult Validate();
    static ResCustom<std::unique_ptr<Image>, ImageError> ImageFactory (ImgType type, const std::string& name);
    static ResCustom<std::unique_ptr<Image>, ImageError> ImageFactory (const std::string& type,
        const std::string& name);

    static std::string GetTypeName (ImgType type)
    {
        return typeNames.GetName (type);
    }
    static ImgType ResolveType (const std::string& type)
    {
        return typeNames.Resolve (type);
    }
    ImgProp ResolveProp (const std::string& name)
    {
        return nameRegistry.Resolve (name);
    }
    std::string GetPropName (ImgProp prop)
    {
        return nameRegistry.GetName (prop);
    }
    std::string GetBootModeName (BootMode mode)
    {
        return bootModes.GetName (mode);
    }

    virtual ~Image() = default;
    // Delete all copy and move constructors and assignment operators
    Image (const Image&) = delete;
    Image& operator= (const Image&) = delete;
    Image (const Image&&) = delete;
    Image& operator= (const Image&&) = delete;

  protected:
    Image (const std::string& name, ImgType type)
    {
        spec.name = name;
        spec.type = type;
    }

    // Helper for common situation where a setter finds an invalid ID
    static ImageError invalidId (const Image& img, const std::string& val)
    {
        return ImageError (ErrorCode::InvalidId, {{"name", img.GetName()}, {"id", val}});
    }

    static BootMode resolveBootMode (const std::string& mode)
    {
        return bootModes.Resolve (mode);
    }

    virtual const ImgConfRegistry& getRegistry() const = 0;
    virtual const std::vector<BackendType> getValidBackends() const = 0;

    ImgSpec spec;
    std::vector<PartRef> partitionNames;
    std::vector<std::unique_ptr<Partition>> parts;
    std::shared_ptr<Backend> backend;
    std::string backendTag;

  private:
    // These functions are the main driver of backend detection logic
    bool checkBackendForImage (BackendType type) const
    {
        const auto& validBackends = getValidBackends();
        return (std::find (validBackends.begin(), validBackends.end(), type) != validBackends.end()) &&
               Backend::IsBackendEnabled (type);
    }
    BackendType firstAvailBackend() const
    {
        const auto& validBackends = getValidBackends();
        // Go through each backend and check if it is enabled
        for (auto backend : validBackends)
        {
            if (Backend::IsBackendEnabled (backend))
                return backend;
        }
        return BackendType::None;
    }

    const static ImgConfRegistry baseRegistry;

    const static NameRegistry<ImgProp> nameRegistry;
    const static NameRegistry<BootMode> bootModes;

    const static EnumArray<ImgType, ImgConstruct, ImgType::Max> factory;
    const static NameRegistry<ImgType> typeNames;
};

#include "include/ImageGet.txx"

#endif
