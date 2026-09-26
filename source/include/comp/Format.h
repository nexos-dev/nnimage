/*
    Format.h - contains disk image format component header
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

#ifndef FORMAT_H
#define FORMAT_H

#include "include/image/ImgComponent.h"

class RawFormatComp : public FormatComp
{
  public:
    RawFormatComp (Image& owner) : FormatComp{FormatType::Raw, owner}
    {}

  protected:
    const CompConfRegistry& getSubRegistry() const override
    {
        return registry;
    }

  private:
    static const CompConfRegistry registry;
};

class Qcow2FormatComp : public FormatComp
{
  public:
    Qcow2FormatComp (Image& owner) : FormatComp{FormatType::Qcow2, owner}
    {}

  protected:
    const CompConfRegistry& getSubRegistry() const override
    {
        return registry;
    }

  private:
    static const CompConfRegistry registry;
};

class IsoFormatComp : public FormatComp
{
  public:
    IsoFormatComp (Image& owner) : FormatComp{FormatType::Iso9660, owner}
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
