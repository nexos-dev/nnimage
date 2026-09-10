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

#include "include/ImgBase.h"
#include "include/ImgError.h"
#include "include/ImgProp.h"
#include "include/CompType.h"
#include "include/EnumArray.h"
#include "CompTypes.h"

#include <functional>
#include <memory>

class Component;
class Image;

using CompConfRegistry = ConfRegistry<ImgProp, Component>;

// Generic image object component
class Component
{
  public:
    virtual ~Component() = default;

    CompType GetType()
    {
        return type;
    }

    ImageResult Set (ImgProp prop, const ImageVal& val);
    ResCustom<std::optional<std::any>, ImageError> Get (ImgProp prop);
    ImageResult SetDefaults();

  protected:
    Component (CompType type, Image& img) : owner{img}, type{type}
    {}
    virtual const CompConfRegistry& getRegistry() const = 0;

    CompType type;
    Image& owner;
};

// Partition layout component

class PartTypeComp : public Component
{
  public:
    virtual ~PartTypeComp() = default;

    static std::unique_ptr<PartTypeComp> Factory (PartType type);

  protected:
    PartTypeComp (PartType type, Image& img) : type{type}, Component{CompType::PartType, img}
    {}

    virtual const CompConfRegistry& getSubRegistry() const = 0;
    const CompConfRegistry& getRegistry() const;

    PartType type;

  private:
    static const NameRegistry<PartType> nameRegistry;
    static const EnumArray<PartType, std::function<std::unique_ptr<PartTypeComp> (Image& img)>, PartType::Max> factory;
};

#include "Components.h"

#endif
