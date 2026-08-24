/*
    Chardet.h - contains wrapper over libchardet for RAII usage
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

#ifndef NNIMAGE_CHARDET_H
#define NNIMAGE_CHARDET_H

#include <config.h>

#ifdef HAVE_CHARDET
#include <chardet/chardet.h>

class Chardet
{
  public:
    Chardet()
    {
        obj = detect_obj_init();
    }
    ~Chardet()
    {
        if (obj)
            detect_obj_free (&obj);
    }

    // Delete copy constructor and assignment operator to prevent copying
    Chardet (const Chardet&) = delete;
    Chardet& operator= (const Chardet&) = delete;
    // Move constructor and assignment operator
    Chardet (Chardet&& other) noexcept : obj (other.obj)
    {
        other.obj = nullptr;
    }
    Chardet& operator= (Chardet&& other) noexcept
    {
        if (this == &other)
            return *this;
        if (obj)
            detect_obj_free (&obj);
        obj = other.obj;
        other.obj = nullptr;
        return *this;
    }
    // API NOTE: if chardet is unavailable, this will always return false and set confidence to 1.0
    // if false is returned, confidence is set to 0 so the caller can differentiate
    bool Detect (const std::string_view data, std::string& encoding, float& confidence)
    {
        confidence = 0.0;    // Reset it
        if (!obj)
            return false;
        if (detect_r (data.data(), data.size(), &obj))
            return false;
        encoding = obj->encoding;
        confidence = obj->confidence;
        return true;
    }

  private:
    DetectObj* obj = nullptr;
};

#else

class Chardet
{
  public:
    Chardet() = default;
    ~Chardet() = default;

    // Delete copy constructor and assignment operator to prevent copying
    Chardet (const Chardet&) = delete;
    Chardet& operator= (const Chardet&) = delete;
    // Move constructor and assignment operator
    Chardet (Chardet&&) noexcept = default;
    Chardet& operator= (Chardet&&) noexcept = default;

    // API NOTE: if chardet is unavailable, this will always return false and set confidence to 1.0
    bool Detect (std::string_view data, std::string& encoding, float& confidence)
    {
        confidence = 1.0;
        return false;    // Detection not available
    }
};

#endif

#endif
