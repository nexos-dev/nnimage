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
    MbrImage (const std::string& name) : Image (name, ImgType::Mbr)
    {}

  protected:
    const ImgConfRegistry& getRegistry() const
    {
        return registry;
    }
    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
    }

  private:
    std::string mbrFile{};
    std::string vbrFile{};

    const static ImgConfRegistry registry;
    inline const static std::vector<BackendType> validBackends = {BackendType::Krun,
        BackendType::Loopback,
        BackendType::Guestfs};
};

class GptImage : public Image
{
  public:
    GptImage (const std::string& name) : Image (name, ImgType::Gpt)
    {}

  protected:
    const ImgConfRegistry& getRegistry() const
    {
        return registry;
    }
    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
    }

  private:
    std::string mbrFile{};
    std::string vbrFile{};

    const static ImgConfRegistry registry;
    inline const static std::vector<BackendType> validBackends = {BackendType::Krun,
        BackendType::Loopback,
        BackendType::Guestfs};
};

// Boot emulation
enum class IsoBootEmu
{
    Noemu,
    Hdd,
    Fdd,
    Max
};

class IsoImage : public Image
{
  public:
    IsoImage (const std::string& name) : Image (name, ImgType::Iso9660)
    {
        spec.fileExt = ".iso";    // Ensure extension is .iso
    }
    ImageResult Validate() override;

  protected:
    const ImgConfRegistry& getRegistry() const
    {
        return registry;
    }
    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
    }

  private:
    std::string bootImageName;
    IsoBootEmu bootEmu = IsoBootEmu::Noemu;
    Image* bootImage = nullptr;

    const static ImgConfRegistry registry;

    const static NameRegistry<IsoBootEmu> bootEmus;
    const static EnumArray<IsoBootEmu, ImgType, IsoBootEmu::Max> bootEmuModes;
    const std::vector<BackendType> validBackends = {BackendType::Xorriso};
};

class FloppyImage : public Image
{
  public:
    FloppyImage (const std::string& name) : Image (name, ImgType::Floppy)
    {}
    ImageResult Validate() override;

  protected:
    const ImgConfRegistry& getRegistry() const
    {
        return registry;
    }

    const std::vector<BackendType> getValidBackends() const
    {
        return validBackends;
    }

  private:
    std::string mbrFile{};

    const static ImgConfRegistry registry;

    inline const static std::vector<int> validSizes = {720, 1440, 2880};

    inline const static std::vector<BackendType> validBackends = {BackendType::Krun,
        BackendType::Loopback,
        BackendType::Guestfs};
};

#endif
