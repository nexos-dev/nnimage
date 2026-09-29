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
#include "include/ImageParser.h"
#include "include/StringHash.h"

#include <memory>
#include <string>
#include <string_view>
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
    std::string format{};
    std::vector<std::string> props{};
    PartitionStrings partSpecs{};

  private:
    Error makeOptionError (std::string_view msg)
    {
        return Error ({ErrorDomain::Option, ErrorCode::InvalidOption}, {{"message", std::string (msg)}});
    }
};

class ImageSet
{
  public:
    std::vector<std::reference_wrapper<Image>> GetImages() const;
    std::optional<std::reference_wrapper<Image>> FindImage (std::string_view name) const;
    std::optional<std::reference_wrapper<Partition>> FindPartition (std::string_view name) const;
    std::shared_ptr<Partition> FindPartitionShared (std::string_view name) const;
    ResNone AddImage (std::unique_ptr<Image> image);
    ResNone AddPartition (std::shared_ptr<Partition> part);
    void Dump();

    template <typename Func>
    void Filter (Func&& filter)
    {
        for (auto it = images.begin(); it != images.end();)
        {
            if (!filter (*it->second))
                it = images.erase (it);
            else
                it++;
        }
    }

    auto begin() const
    {
        return images.begin();
    }
    auto end() const
    {
        return images.end();
    }
    auto size() const
    {
        return images.size();
    }

  private:
    // These contain all the images/partitions that have been parsed
    std::unordered_map<std::string, std::unique_ptr<Image>, StringHash, std::equal_to<>> images{};
    std::unordered_map<std::string, std::shared_ptr<Partition>, StringHash, std::equal_to<>> partitions{};
};

class Frontend
{
  public:
    Frontend() = default;
    Frontend (FrontendOptions& opts) : opts{opts}
    {}
    virtual ~Frontend() = default;
    virtual ResNone Parse() = 0;

    ImageSet& GetSet()
    {
        return images;
    }

  protected:
    FrontendOptions opts;
    ImageSet images;
    // References to partitions
    std::vector<GenericRef<Image>> partRefs{};
};

class SimpleLexer;

// Frontend for specifying an image on the command line
class ImageCmd : public Frontend
{
  public:
    ImageCmd() = default;
    ImageCmd (FrontendOptions& opts) : Frontend (opts)
    {}
    ResNone Parse();

  private:
    ResNone assertIsEnd (LexToken& tok);
    Result<ImageVal> convertStr (std::string val);
    Result<LexToken> getOneToken (std::string val);
    ResNone assertTokenEnd (SimpleLexer& lex);

    template <typename T>
    Result<T> getTokenValue (std::string val, TokenType type);

    ResNone processNumId (Image& img, ImgProp prop, std::string val);
    ResNone processId (Image& img, ImgProp prop, std::string val);

    ResNone processProps (Image& img);
    ResNone processPartitions (Image& img);

    Error& badArgument (Error& e, std::string_view prop)
    {
        return e.Add ({ErrorDomain::Option, ErrorCode::UnableToProcessOption}, {{"option", std::string (prop)}});
    }
};

// Frontend for images coming from a file
class ImageConf : public Frontend
{
  public:
    ImageConf() = default;
    ImageConf (FrontendOptions& opts) : Frontend (opts)
    {}
    ResNone Parse();

  private:
    Result<std::string> readConfFile();

    Result<std::unique_ptr<Image>> createImage (ImgParseBlock block);
    Result<std::shared_ptr<Partition>> createPartition (ImgParseBlock block);

    ResNone addPartitionNames (Image& img, const ImageVal& val);

    ResNone resolvePartRefs();
    ResNone resolveImgRefs();

    Error parseFailed (Error& e, ErrorLog verbosity = ErrorLog::Normal)
    {
        return e.Add (ImageError::Make (ErrorCode::ImgParseFailed, {}, verbosity));
    }

    ImageParser parser;
};

#endif
