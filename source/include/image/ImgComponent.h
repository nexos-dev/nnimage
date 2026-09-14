/*
    ImgComponent.h - contains component base
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

#ifndef IMGCOMPONENT_H
#define IMGCOMPONENT_H

#include "include/image/ImgBase.h"
#include "include/image/ImgError.h"
#include "include/image/ImgProp.h"
#include "include/CompType.h"
#include "include/EnumArray.h"
#include "CompTypes.h"

#include <functional>
#include <memory>

class Component;
class Image;

using CompConfRegistry = ConfRegistry<ImgProp, Component>;

// Generic image object component
class Component : public RegElement<Component, ImgProp, CompConfRegistry>
{
  public:
    virtual ~Component() = default;

    using RegElement<Component, ImgProp, CompConfRegistry>::Get;
    using RegElement<Component, ImgProp, CompConfRegistry>::IsSet;
    using RegElement<Component, ImgProp, CompConfRegistry>::Set;
    using RegElement<Component, ImgProp, CompConfRegistry>::SetDefaults;

    CompType GetType()
    {
        return type;
    }

    Image& GetOwner()
    {
        return owner;
    }

    virtual ImageResult Validate() = 0;

  protected:
    Component (CompType type, Image& img)
        : RegElement{ErrorCode::InvalidImgProp, ErrorCode::ImgMissingProp}, owner{img}, type{type}
    {}

    const CompConfRegistry& getRegistry() override;
    virtual const CompConfRegistry& getMainRegistry() const = 0;
    virtual const CompConfRegistry& getSubRegistry() const = 0;

    template <typename Derived>
    static Derived& derived (Component& comp)
    {
        // Slow? yes. But it's good for checking on debug builds
        assert (typeid (comp) == typeid (Derived));
        return static_cast<Derived&> (comp);
    }

    Image& owner;

  private:
    const std::string& getRegElementName() const override;
    const std::string& getPropName (ImgProp prop) const override;

    // The union of the main and sub registries
    CompConfRegistry mergedRegistry;

    CompType type;
};

template <typename Enum, class Class>
using CompFactoryTable = EnumArray<Enum, std::function<std::unique_ptr<Class> (Image&)>, Enum::Max>;

// Partition layout component

class PartTypeComp : public Component
{
  public:
    virtual ~PartTypeComp() = default;

    PartType GetPartType() const
    {
        return type;
    }

    static std::unique_ptr<PartTypeComp> Factory (PartType type, Image& owner);
    static std::unique_ptr<PartTypeComp> Factory (std::string_view type, Image& owner);
    virtual ImageResult Validate() override;

  protected:
    PartTypeComp (PartType type, Image& img) : type{type}, Component{CompType::PartType, img}
    {}

    const CompConfRegistry& getMainRegistry() const override
    {
        return registry;
    }
    virtual const CompConfRegistry& getSubRegistry() const override = 0;

    std::string getTypeName (PartType type)
    {
        return nameRegistry.GetName (type);
    }

    PartType type;

  private:
    static const CompConfRegistry registry;

    static const NameRegistry<PartType> nameRegistry;
    static const CompFactoryTable<PartType, PartTypeComp> factory;
};

// Bootloader type component

class BootLoadComp : public Component
{
  public:
    virtual ~BootLoadComp() = default;

    BootLoadType GetBootType() const
    {
        return type;
    }

    static std::unique_ptr<BootLoadComp> Factory (BootLoadType type, Image& owner);
    static std::unique_ptr<BootLoadComp> Factory (std::string_view type, Image& owner);
    virtual ImageResult Validate() override;

  protected:
    BootLoadComp (BootLoadType type, Image& img) : type{type}, Component{CompType::Boot, img}
    {}

    const CompConfRegistry& getMainRegistry() const override
    {
        return registry;
    }
    virtual const CompConfRegistry& getSubRegistry() const override = 0;

    std::string getTypeName (BootLoadType type)
    {
        return nameRegistry.GetName (type);
    }

    BootLoadType type;

  private:
    static const CompConfRegistry registry;

    static const NameRegistry<BootLoadType> nameRegistry;
    static const CompFactoryTable<BootLoadType, BootLoadComp> bootFactory;
};

#include "Components.h"

#endif
