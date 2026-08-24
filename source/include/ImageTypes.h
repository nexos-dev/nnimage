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

#ifndef NNIMAGE_IMAGETYPES_H
#define NNIMAGE_IMAGETYPES_H

#include "include/Image.h"

// Image type classes
class MbrImage : public Image
{
  public:
    MbrImage (const std::string& name) : Image (name, ImageType::Mbr)
    {}

  protected:
    const std::unordered_map<ImgProp, ImgConfItem>& getRegistry()
    {
        return registry;
    }
    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
    }

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<ImgProp, ImgConfItem> registry;
    // Valid backends
    const std::vector<BackendType> validBackends = {BackendType::Krun,
                                                    BackendType::Loopback,
                                                    BackendType::Guestfs};
};

class GptImage : public Image
{
  public:
    GptImage (const std::string& name) : Image (name, ImageType::Gpt)
    {}

  protected:
    const std::unordered_map<ImgProp, ImgConfItem>& getRegistry()
    {
        return registry;
    }
    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
    }

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<ImgProp, ImgConfItem> registry;
    // Valid backends
    const std::vector<BackendType> validBackends = {BackendType::Krun,
                                                    BackendType::Loopback,
                                                    BackendType::Guestfs};
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
    {
        spec.fileExt = ".iso";    // Ensure extension is .iso
    }

  protected:
    const std::unordered_map<ImgProp, ImgConfItem>& getRegistry()
    {
        return registry;
    }
    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
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
    // Valid backends
    const std::vector<BackendType> validBackends = {BackendType::Xorriso};
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
    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
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
    // Valid backends
    const std::vector<BackendType> validBackends = {BackendType::Krun,
                                                    BackendType::Loopback,
                                                    BackendType::Guestfs};
};

#endif
