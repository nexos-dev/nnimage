/*
    Frontend.h - contains frontend classes that create image configurations
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

#ifndef FRONTEND_H
#define FRONTEND_H

#include "include/Error.h"
#include "include/Image.h"
#include "include/Options.h"
#include "include/OptionParser.h"
#include "include/SimpleLexer.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using PartitionStrings = CommaString;

// NOTE: there is not a different options class for derived classes because we need access to all options at
// once to determine which frontend to use
class Frontend;
class FrontendOptions : public Options
{
  public:
    void CollectOptions (OptionsParser& opts);
    ResNone ValidateOptions();
    std::unique_ptr<Frontend> CreateFrontend (OptionsParser& opts);

    std::string confFile{};
    std::string confEnc{};

    std::string imgSize{};
    std::string bootMode{};
    std::string imgType{};
    std::vector<std::string> props{};
    PartitionStrings partSpecs{};

  private:
    Error makeOptionError (const std::string& msg)
    {
        return Error ({ErrorDomain::Option, ErrorCode::InvalidOption}, msg);
    }
};

class Frontend
{
  public:
    Frontend() = default;
    Frontend (FrontendOptions& opts) : opts{opts}
    {}
    virtual ~Frontend() = default;
    virtual ResNone Parse() = 0;
    std::vector<std::unique_ptr<Image>> GetImages()
    {
        std::vector<std::unique_ptr<Image>> vec;
        vec.reserve (images.size());
        for (auto& [key, ptr] : images)
            vec.push_back (std::move (ptr));
        // Clear the maps as we are done with them now
        images.clear();
        partitions.clear();
        return vec;
    }

  protected:
    FrontendOptions opts;
    // These contain all the images/partitions that have been parsed
    std::unordered_map<std::string, std::unique_ptr<Image>> images{};
    std::unordered_map<std::string, std::unique_ptr<Partition>> partitions{};
    // These are any references between them. They get resolved at the end of parsing
    std::vector<GenericRef<Image>> imageRefs{};
    std::vector<GenericRef<Partition>> partRefs{};
};

// Frontend for specifying an image on the command line
class ImageCmd : public Frontend
{
  public:
    ImageCmd() = default;
    ImageCmd (FrontendOptions& opts) : Frontend (opts)
    {}
    ResNone Parse();

  private:
    template <typename T>
    Result<T> getToken (const std::string& val, const std::string& prop, TokenType type);
    Result<ImageNumId> getSize();
};

// Frontend for images coming from a file
class ImageConf : public Frontend
{
  public:
    ImageConf() = default;
    ImageConf (FrontendOptions& opts) : Frontend (opts)
    {}
    ResNone Parse();
};

#endif
