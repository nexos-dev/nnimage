/*
    BootLoad.h - contains bootloader component header
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

#ifndef BOOTLOAD_H
#define BOOTLOAD_H

#include "include/image/ImgComponent.h"

class BootNoneComp : public BootLoadComp
{
  public:
    BootNoneComp (Image& owner) : BootLoadComp{BootLoadType::None, owner}
    {}

  protected:
    const CompConfRegistry& getSubRegistry() const override
    {
        return registry;
    }

  private:
    static const CompConfRegistry registry;
};

class GrubBootComp : public BootLoadComp
{
  public:
    GrubBootComp (Image& owner) : BootLoadComp{BootLoadType::Grub, owner}
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
