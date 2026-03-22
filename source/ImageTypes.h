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

// Defined image types
enum class ImageType
{
    Mbr,
    Gpt,
    Iso9660,
    Floppy
};

// Image type classes
class MbrImage : public Image
{
  public:
    MbrImage (const std::string& name) : Image (name, ImageType::Mbr)
    {
    }

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<std::string, Setter> registry;
};

class GptImage : public Image
{
  public:
    GptImage (const std::string& name) : Image (name, ImageType::Gpt)
    {
    }

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<std::string, Setter> registry;
};

class IsoImage : public Image
{
  public:
    IsoImage (const std::string& name) : Image (name, ImageType::Iso9660)
    {
    }

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<std::string, Setter> registry;
};

class FloppyImage : public Image
{
  public:
    FloppyImage (const std::string& name) : Image (name, ImageType::Floppy)
    {
    }

  private:
    // Gets numeric boot mode from string boot mode
    BootMode getBootMode (const std::string& modeStr);
    // List of valid boot modes for image
    const static std::unordered_map<std::string, BootMode> validBootModes;
    // Registry of configuration keys
    const static std::unordered_map<std::string, Setter> registry;
};

#endif
