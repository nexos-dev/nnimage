/*
    TextReader.h - contains class to read text files in an encoding neutral manner
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

#ifndef TEXTREADER_H
#define TEXTREADER_H

#include "include/Chardet.h"
#include "include/Iconv.h"
#include "include/Error.h"
#include "MemoryMapped.h"

// This class is designed solely for reading in whole text files at once
class TextReader
{
  public:
    TextReader() = default;
    TextReader (std::filesystem::path file, const std::string& forceEnc = "") : file{file}, fileEnc{forceEnc}
    {
        if (!handle.open (file.c_str()))
        {
            throw ErrorException (Error ({ErrorDomain::None, ErrorCode::FileError},
                                         "%s: %s",
                                         std::string (file.filename()),
                                         handle.getError()));
        }
    }
    TextReader (const std::string& file, const std::string& forceEnc = "")
        : TextReader (std::filesystem::path (file), forceEnc)
    {}
    ResNone Read (std::string& buf, bool requireEnc = false)
    {
        // Make sure handle is valid
        if (!handle.isValid())
        {
            throw ErrorException (
                Error ({ErrorDomain::None, ErrorCode::Internal}, "attempt to read from unopened file"));
        }
        // Grab data
        const std::string_view data = std::string_view (reinterpret_cast<const char*> (handle.getData()));
        // Prepare encoding detection
        Chardet chardet;
        std::string enc = "";
        float confidence = 0;
        if (!fileEnc.empty())
            enc = fileEnc;    // Force it
        else
        {
            // Attempt to detect it
            if (!chardet.Detect (data, enc, confidence) || confidence < 0.5)
            {
                if (!requireEnc)
                {
                    // If chardet couldn't detect the encoding and the user didn't specify one, warn the user
                    // and force ASCII
                    // TODO: error output class isn't made yet
                    enc = "ASCII";
                    confidence = 1.0;    // Force it as ASCII is compatible with everything
                }
                else
                {
                    // If the user didn't want us to force the encoding, then their crazy but
                    // we'll just do as they say
                    return Error ({ErrorDomain::None, ErrorCode::EncMismatch},
                                  "Unable to detect character encoding of file \"%s\"",
                                  file.filename());
                }
            }
        }
        // We now have a valid encoding, now we need to convert it
        // And then we are done
        Iconv conv (enc, "UTF-8");
        if (!conv.Convert (data, buf))
        {
            return Error ({ErrorDomain::None, ErrorCode::SysFailure},
                          "failed to convert file: %s",
                          strerror (errno));
        }
        // We are done as iconv put it in the output buffer for us
        return Success();
    }

  private:
    std::filesystem::path file;
    const std::string fileEnc;
    MemoryMapped handle;
};

#endif
