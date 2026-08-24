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

class Backend;

// TODO this probably should go into ConfParser.h
enum class ConfErrorType
{
    Ok,
    BadType,
    NoName,
    BadProp,
    ExtraVals,
    PropRequired,
    WrongType,
    UnrecognizedId,
    BadNumId,
    UnrecognizedVal,
    SysError
};

struct ConfError
{
    ConfError()
    {}
    ConfError (ConfErrorType type, const ParseProp& prop) : code{type}, msg{prop.name}, line{prop.line}
    {}
    ConfError (ConfErrorType type, const std::string& msg, int line) : code{type}, msg{msg}, line{line}
    {}
    ConfErrorType code = ConfErrorType::Ok;
    std::string msg;
    int line;
};

// Used by action logic to store conf file stuff
struct ImgConfFile
{
    std::string fileName;
    std::string fileEnc;
};

class Image;
class Partition;
class ImageConf
{
  public:
    ImageConf() = delete;
    ImageConf (const ImgConfFile& file) : confFile{file}
    {}
    std::vector<std::unique_ptr<Image>>& GetImages()
    {
        return images;
    }
    std::unique_ptr<Partition> GetPartition (const std::string& name)
    {
        auto it = this->parts.find (name);
        if (it == this->parts.end())
            return nullptr;
        return std::move (it->second);
    }
    bool ParseFile();
    ParseProp& GetProp (ParseBlock& block,
                        const std::string& name,
                        ConfType expectedType,
                        int maxVals,
                        ConfError& result);
    bool ValidateProp (const ParseProp& prop, ConfType expectedsType, int maxVal, ConfError& result);
    bool GetVal (const ParseProp& prop, ConfVal& out, int idx);

  private:
    bool openConfFile (std::string& contents);
    bool processImageBlock (ParseBlock& block);
    bool processPartitionBlock (ParseBlock& block);
    void parseError (ConfErrorType error, int line, const std::string& extra);
    bool addPartitions (Image* img, const ParseProp& prop);
    void removeProp (ParseBlock& block, ParseProp& prop);
    std::vector<std::unique_ptr<Image>> images;
    std::map<std::string, std::unique_ptr<Partition>> parts;
    ImgConfFile confFile;
};

using ImgConfSetter = std::function<void (Image&, const ConfVal&, ConfErrorType&)>;
using PartConfSetter = std::function<void (Partition&, const ConfVal&, ConfErrorType&)>;

struct ImgConfItem
{
    ConfType type;
    ImgConfSetter setter;
};

struct PartConfItem
{
    ConfType type;
    PartConfSetter setter;
};

// NOTE: this struct is in NO WAY copyable or movable and ALWAYS is const
struct PartSpec
{
    std::string name;
    std::string fs;
    std::string prefix;
    int64_t start = -1;
    int64_t size = -1;
    bool isBoot = false;
    // Delete all copy and move constructors and assignment operators
    PartSpec (const PartSpec&) = delete;
    PartSpec& operator= (const PartSpec&) = delete;
    PartSpec (const PartSpec&&) = delete;
    PartSpec& operator= (const PartSpec&&) = delete;
    PartSpec() = default;
    ~PartSpec() = default;
};

enum class PartProp
{
    None,
    Start,
    Size,
    Format,
    Prefix,
    IsBoot
};

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
    void SetConf (const std::string& name, const ConfVal& val, ConfError& e);
    void SetConf (PartProp key, const ConfVal& val, ConfError& e);
    PartProp ResolveName (const std::string& name);
    const PartSpec& GetSpec() const
    {
        return spec;
    }

  private:
    const std::string getPropName (PartProp prop);
    PartSpec spec;
    const static std::unordered_map<PartProp, PartConfItem> registry;
    const static std::unordered_map<std::string, PartProp> nameRegistry;
};

class PartRef
{
  public:
    PartRef (const std::string& name, int line) : name{name}, line{line}
    {}
    const std::string GetName() const
    {
        return name;
    }
    int GetLine() const
    {
        return line;
    }

  private:
    const std::string name;
    int line;
};

enum class ImageType
{
    Mbr,
    Gpt,
    Iso9660,
    Floppy,
    Error
};

enum class BootMode
{
    None,
    Bios,
    Efi,
    Error
};

enum class ImgProp
{
    None,
    Size,
    BootMode,
    BootEmu,
    BootImage
};

// NOTE NOTE NOTE: this struct is in NO WAY copyable or movable and ALWAYS is const
// If any code ever tries to copy or move it, many things would fail
// It is STRICTLY NON COPYABLE and NON MOVABLE
// Do not attempt to do so. If you do, gcc/clang will find your home and make your life miserable
// NOTE2: It is possible that all of this should be behind getters instead on in a struct,
// but that would add a ton of boilerplate code
struct ImgSpec
{
    std::string name;
    std::string fileExt = ".img";
    std::string backendTag;
    ImageType type = ImageType::Gpt;
    int64_t size = -1;
    BootMode bootMode = BootMode::None;
    mutable std::mutex lock;
    ImgSpec (const ImgSpec&) = delete;
    ImgSpec& operator= (const ImgSpec&) = delete;
    ImgSpec (const ImgSpec&&) = delete;
    ImgSpec& operator= (const ImgSpec&&) = delete;
    ImgSpec() = default;
    ~ImgSpec() = default;
};

class Image
{
  public:
    Image (const std::string& name, ImageType type)
    {
        spec.name = name;
        spec.type = type;
    }
    const std::string& GetName() const
    {
        return spec.name;
    }
    ImageType GetType() const
    {
        return spec.type;
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
    BackendType GetBackendType (BackendType suggestion) const;
    void SetConf (const std::string& name, const ConfVal& val, ConfError& e);
    void SetConf (ImgProp prop, const ConfVal& val, ConfError& e);
    void AddPartition (PartRef& ref)
    {
        partitionNames.push_back (ref);
    }
    void AddPartition (std::unique_ptr<Partition> part)
    {
        parts.push_back (std::move (part));
    }
    bool ResolvePartitions (ConfError& e);
    const ImgSpec& GetSpec() const
    {
        return spec;
    }
    const std::vector<std::unique_ptr<Partition>>& GetPartitions() const
    {
        return parts;
    }
    // Used by backends to identify the image, e.g. the block device name
    void SetBackendTag (const std::string& tag)
    {
        spec.backendTag = tag;
    }
    virtual bool Validate();
    static std::unique_ptr<Image> ImageFactory (const std::string& type, const std::string& name);
    static int64_t NormalizeNumId (const ConfNumId& numId);
    static ImgProp ResolveProp (const std::string& name);
    static const std::string GetTypeName (ImageType type);
    virtual ~Image() = default;
    // Delete all copy and move constructors and assignment operators
    Image (const Image&) = delete;
    Image& operator= (const Image&) = delete;
    Image (const Image&&) = delete;
    Image& operator= (const Image&&) = delete;

  protected:
    virtual BootMode getBootMode (const std::string& modeStr) = 0;
    virtual const std::unordered_map<ImgProp, ImgConfItem>& getRegistry() = 0;
    virtual const std::vector<BackendType> getValidBackends() const = 0;
    ImgSpec spec;
    std::vector<PartRef> partitionNames;
    std::vector<std::unique_ptr<Partition>> parts;
    std::shared_ptr<Backend> backend;

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
    const std::string getPropName (ImgProp prop);
    const static std::unordered_map<ImgProp, ImgConfItem> baseRegistry;
    const static std::unordered_map<std::string, ImgProp> nameRegistry;
};

#endif
