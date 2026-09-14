/*
    Chardet.h - contains wrapper over uchardet for RAII usage
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

#include <string>
#include <string_view>

#ifdef HAVE_UCHARDET
#include <uchardet/uchardet.h>

class Chardet
{
  public:
    Chardet()
    {
        obj = uchardet_new();
    }
    ~Chardet()
    {
        if (obj)
            uchardet_delete (obj);
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
            uchardet_delete (obj);
        obj = other.obj;
        other.obj = nullptr;
        return *this;
    }
    bool Detect (const std::string_view data, std::string& encoding)
    {
        if (!obj)
            return false;
        if (uchardet_handle_data (obj, data.data(), data.size()))
            return false;
        uchardet_data_end (obj);
        const char* charset = uchardet_get_charset (obj);
        if (!charset || *charset == '\0')
            return false;
        encoding = charset;
        return true;
    }

  private:
    uchardet_t obj = nullptr;
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

    bool Detect (std::string_view data, std::string& encoding)
    {
        return false;    // Detection not available
    }
};

#endif

#endif
