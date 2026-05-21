/*
    ImageTypes.h - contains image type definitions
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

#ifndef IMAGETYPES_H
#define IMAGETYPES_H

#include "nnimage.h"

// Defined image properties
enum class ImgProp
{
    // Generic
    None,
    Size,
    BootMode,
    // ISO9660
    BootEmu,
    BootImage
};

// Defined partition properties
enum class PartProp
{
    None,
    Start,
    Size,
    Format,
    Prefix,
    IsBoot
};

// Image type classes
class MbrImage : public Image
{
  public:
    MbrImage (const std::string& name) : Image (name, ImageType::Mbr)
    {}
    BackendType GetBackend (BackendType suggestion)
    {
        if (suggestion != BackendType::Isofs)
            return suggestion;
        return BACKEND_DEFAULT;
    }

  protected:
    const std::unordered_map<ImgProp, ImgConfItem>& getRegistry()
    {
        return registry;
    }
    bool Validate() override;

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<ImgProp, ImgConfItem> registry;
};

class GptImage : public Image
{
  public:
    GptImage (const std::string& name) : Image (name, ImageType::Gpt)
    {}
    BackendType GetBackend (BackendType suggestion)
    {
        if (suggestion != BackendType::Isofs)
            return suggestion;
        return BACKEND_DEFAULT;
    }

  protected:
    const std::unordered_map<ImgProp, ImgConfItem>& getRegistry()
    {
        return registry;
    }
    bool Validate() override;

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<ImgProp, ImgConfItem> registry;
};

// Boot emulation
enum class IsoBootEmu
{
    Noemu,
    Hdd,
    Fdd
};

class IsoImage : public Image
{
  public:
    IsoImage (const std::string& name) : Image (name, ImageType::Iso9660)
    {}
    BackendType GetBackend (BackendType suggestion)
    {
        return BackendType::Isofs;
    }

  protected:
    const std::unordered_map<ImgProp, ImgConfItem>& getRegistry()
    {
        return registry;
    }
    bool Validate() override;

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<ImgProp, ImgConfItem> registry;
    // Data fields
    IsoBootEmu bootEmu = IsoBootEmu::Noemu;    // Selected boot emulation
    std::string bootImageName;                 // Boot image file name for hdd and fdd emus
    Image* bootImage;
    // Valid boot emulations
    const static std::unordered_map<std::string, IsoBootEmu> bootEmus;
    // Boot emulation to boot image type mapping
    const static std::unordered_map<IsoBootEmu, ImageType> bootEmuModes;
};

class FloppyImage : public Image
{
  public:
    FloppyImage (const std::string& name) : Image (name, ImageType::Floppy)
    {}

  protected:
    const std::unordered_map<ImgProp, ImgConfItem>& getRegistry()
    {
        return registry;
    }
    BackendType GetBackend (BackendType suggestion)
    {
        if (suggestion != BackendType::Isofs)
            return suggestion;
        return BACKEND_DEFAULT;
    }
    bool Validate() override;

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<ImgProp, ImgConfItem> registry;
    // Valid sizes for a floppy image
    const static std::vector<int> validSizes;
};

#endif
