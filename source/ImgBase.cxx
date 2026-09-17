/*
    ImgBase.cxx - contains base classes of image layer
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

#include "include/image/ImgBase.h"
#include "include/SimpleLexer.h"

template <class... Ts>
struct overloaded : Ts...
{
    using Ts::operator()...;
};

Result<ImageVal> ImageVal::FromToken (const LexToken& tok)
{
    return std::visit (overloaded{[&] (const std::string& x) -> Result<ImageVal> {
                                      if (tok.type == TokenType::Identifier)
                                          return ImageVal (ImageId (x), tok.line);
                                      else
                                          return ImageVal (x, tok.line);
                                  },
                           [&] (uint64_t x) -> Result<ImageVal> { return ImageVal (x, tok.line); },
                           [&] (bool x) -> Result<ImageVal> { return ImageVal (x, tok.line); },
                           [&] (const LexNumId& x) -> Result<ImageVal> {
                               ImageNumId numId = ImageNumId (x.num, x.id);
                               auto res = numId.Parse();
                               if (!res)
                                   return res.Error();
                               return ImageVal (numId);
                           }},
        tok.val);
}

ImageVal ImageVal::Cast (ImageValIdx wantedType) const
{
    return std::visit (overloaded{[&] (const ImageId& x) -> ImageVal {
                                      if (wantedType == ImageVal::GetTypeIndex<std::string>())
                                          return ImageVal (std::string (x), line);
                                      return ImageVal::Invalid;
                                  },
                           [&] (auto&&) -> ImageVal { return ImageVal::Invalid; }},
        val);
}

void ImageError::makeMessage (ErrorFrame& frame)
{
    std::stringstream msg;
    // Check if we have a file/line
    if (auto it = keys.find ("file"); it != keys.end())
    {
        msg << getString (it) << ":";
        // Check for a line now
        if (auto it = keys.find ("line"); it != keys.end())
            msg << getString (it) << ": ";
        else
            msg << " ";    // Still place a space
    }

    switch (frame.code)
    {
        // NOTE: all the below assertKeys calls only do anything on debug builds. That shouldn't be an issue
        case ErrorCode::NameMissing:
            assertKeys ({"block_type"});
            msg << "Name required for block type \"" << getString ("block_type") << "\"";
            break;
        case ErrorCode::InvalidImgType:
            assertKeys ({"type"});
            msg << "Invalid image type \"" << getString ("type") << "\" specified on image" << getName();
            break;
        case ErrorCode::InvalidImgProp:
            assertKeys ({"prop"});
            msg << "Unrecognized property \"" << getString ("prop") << "\" specified on image" << getName();
            break;
        case ErrorCode::BadFloppySize:
            msg << "Floppy disc" << getName() << " must have size 720K, 1.44M, or 2.88M";
            break;
        case ErrorCode::InvalidPartProp:
            assertKeys ({"prop"});
            msg << "Unrecognized property \"" << getString ("prop") << "\" specified on partition" << getName();
            break;
        case ErrorCode::PropTypeMismatch:
            assertKeys ({"prop"});
            msg << "Invalid type specified on property \"" << getString ("prop") << "\"";
            break;
        case ErrorCode::InvalidId:
            assertKeys ({"id", "prop"});
            msg << "Invalid ID \"" << getString ("id") << "\" specified for property \"" << getString ("prop")
                << "\" on image" << getName();
            break;
        case ErrorCode::ImgMissingProp:
            assertKeys ({"prop"});
            msg << "Missing required property \"" << getString ("prop") << "\" on image " << getName();
            break;
        case ErrorCode::PartMissingProp:
            assertKeys ({"prop"});
            msg << "Missing required property \"" << getString ("prop") << "\" on partition " << getName();
            break;
        case ErrorCode::MissingPart:
            msg << "Image" << getName() << " requires at least one partition";
            break;
        case ErrorCode::DuplicateImage:
            msg << "Image" << getName() << " already exists";
            break;
        case ErrorCode::CompNotLoaded:
            msg << "Attempt to use unloaded component on image" << getName();
            break;
        default:
            msg << frame.msg;
    }
    frame.msg = msg.str();
}

const std::unordered_map<std::string, size_t> ImageNumId::mulMap = {{"B", 1},
    {"KiB", 1024},
    {"KB", 1000},
    {"MiB", 1024 * 1024},
    {"MB", 1000 * 1000},
    {"GiB", 1024 * 1024 * 1024},
    {"GB", 1000 * 1000 * 1000},
    {"TiB", static_cast<size_t> (1024) * 1024 * 1024 * 1024},
    {"TB", static_cast<size_t> (1000) * 1000 * 1000 * 1000}};
