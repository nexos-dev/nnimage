/*
    Iconv.h - Header file for iconv wrapper functions
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

#ifndef ICONV_H
#define ICONV_H

#include <iconv.h>

#include <string>
#include <string_view>
#include <vector>

class Iconv
{
  public:
    Iconv (const std::string& fromEnc, const std::string& toEnc)
    {
        cd = iconv_open (toEnc.c_str(), fromEnc.c_str());
        if (cd == (iconv_t) -1)
        {
            cd = (iconv_t) -1;
            return;
        }
    }
    ~Iconv()
    {
        if (cd != (iconv_t) -1)
        {
            iconv_close (cd);
        }
    }

    // Delete copy constructor and assignment operator to prevent copying
    Iconv (const Iconv&) = delete;
    Iconv& operator= (const Iconv&) = delete;
    // Move constructor and assignment operator
    Iconv (Iconv&& other) noexcept : cd (other.cd)
    {
        other.cd = (iconv_t) -1;
    }
    Iconv& operator= (Iconv&& other) noexcept
    {
        if (this == &other)
            return *this;
        if (cd != (iconv_t) -1)
            iconv_close (cd);

        cd = other.cd;
        other.cd = (iconv_t) -1;
        return *this;
    }
    bool Convert (std::string_view input, std::string& out)
    {
        // Ensure input is non-empty
        if (input.empty() || cd == (iconv_t) -1)
            return false;

        // Reset iconv
        iconv (cd, nullptr, nullptr, nullptr, nullptr);

        size_t inBytesLeft = input.size();
        size_t outBytesLeft = inBytesLeft * 4;    // Max possible size if using UTF-8/UTF-32
        std::vector<char> outBuffer (outBytesLeft);

        char* inBuf = const_cast<char*> (input.data());
        char* outBuf = outBuffer.data();

        size_t result = iconv (cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
        if (result == (size_t) -1)
            return false;
        // NOTE: copies it here
        out = std::string (outBuffer.data(), outBuffer.size() - outBytesLeft);
        return true;
    }

  private:
    iconv_t cd;
};

#endif
