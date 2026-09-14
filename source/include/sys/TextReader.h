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

#include "include/sys/Chardet.h"
#include "include/sys/Iconv.h"
#include "include/Error.h"
#include "MemoryMapped.h"

#include <filesystem>
#include <string>
#include <string_view>

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
                "{}: {}",
                file.filename().string(),
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
            throw std::runtime_error ("attempt to read from unopened file");

        // Grab data
        const std::string_view data (reinterpret_cast<const char*> (handle.getData()), handle.mappedSize());
        // Prepare encoding detection
        Chardet chardet;
        std::string enc = "";
        if (!fileEnc.empty())
            enc = fileEnc;    // Force it
        else
        {
            // Attempt to detect it
            if (!chardet.Detect (data, enc))
            {
                if (!requireEnc)
                {
                    // If chardet couldn't detect the encoding and the user didn't specify one, warn the user
                    // and force ASCII
                    Error err =
                        Error ({ErrorDomain::None, ErrorCode::EncMismatch, ErrorLog::Normal, ErrorSeverity::Warning},
                            "unable to detect character set for file {}, assuming ASCII",
                            file.string());
                    ErrorOutput::The()->Report (err);

                    enc = "ASCII";
                }
                else
                {
                    // If the user didn't want us to force the encoding, then just do as they say
                    return Error ({ErrorDomain::None, ErrorCode::EncMismatch},
                        "Unable to detect character encoding of file \"{}\"",
                        file.filename().string());
                }
            }
        }
        // We now have a valid encoding, now we need to convert it
        // And then we are done
        Iconv conv (enc, "UTF-8");
        if (!conv.Convert (data, buf))
        {
            return Error ({ErrorDomain::None, ErrorCode::SysFailure}, "failed to convert file: {}", strerror (errno));
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
