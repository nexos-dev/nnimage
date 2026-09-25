/*
    PartComp.h - contains partition layout components
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

#ifndef PARTCOMP_H
#define PARTCOMP_H

#include "include/image/ImgComponent.h"

class MbrPartComp final : public PartTypeComp
{
  public:
    MbrPartComp (Image& img) : PartTypeComp{PartType::Mbr, img}
    {}

  protected:
    const CompConfRegistry& getSubRegistry() const override
    {
        return registry;
    }

  private:
    static const CompConfRegistry registry;
};

class GptPartComp final : public PartTypeComp
{
  public:
    GptPartComp (Image& img) : PartTypeComp{PartType::Gpt, img}
    {}

  protected:
    const CompConfRegistry& getSubRegistry() const override
    {
        return registry;
    }

  private:
    static const CompConfRegistry registry;
};

enum class IsoBootEmu
{
    NoEmu,
    Hdd,
    Fdd,
    Max
};

class IsoPartComp final : public PartTypeComp
{
  public:
    IsoPartComp (Image& img) : PartTypeComp{PartType::Iso9660, img}
    {}

    ResNone Validate() override;

  protected:
    const CompConfRegistry& getSubRegistry() const override
    {
        return registry;
    }

  private:
    IsoBootEmu bootEmu = IsoBootEmu::Max;
    Image* bootImage = nullptr;

    // Boot emulation registry
    static const NameRegistry<IsoBootEmu> bootEmus;

    // Mapping of bootEmu->accepted partition type for boot image
    inline static const EnumArray<IsoBootEmu, std::vector<PartType>, IsoBootEmu::Max> validBootImage = {
        {IsoBootEmu::Hdd, {PartType::Mbr, PartType::Gpt}},
        {IsoBootEmu::Fdd, {PartType::Floppy}}};

    static const CompConfRegistry registry;
};

class FloppyPartComp final : public PartTypeComp
{
  public:
    FloppyPartComp (Image& img) : PartTypeComp{PartType::Floppy, img}
    {}

  protected:
    const CompConfRegistry& getSubRegistry() const override
    {
        return registry;
    }

  private:
    static const CompConfRegistry registry;
};

#endif
