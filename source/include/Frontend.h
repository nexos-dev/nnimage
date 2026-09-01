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

// NOTE: this is here to prevent cxxopts from comma-splitting a partition spec. See Frontend.cxx for more
// clarity
struct PartitionStrings
{
    std::vector<std::string> values;
};

// NOTE: there is not a different options class for derived classes because we need access to all options at
// once to determine which frontend to use
class Frontend;
class FrontendOptions : public Options
{
  public:
    void CollectOptions (cxxopts::Options& opts);
    ResNone ValidateOptions();
    static std::unique_ptr<Frontend> CreateFrontend();

    std::string confFile{};
    std::string confEnc{};

    std::string imgSize{};
    std::string bootMode{};
    std::vector<std::string> props{};
    PartitionStrings partSpecs{};
};

class Frontend
{
  public:
    Frontend() = default;
    Frontend (FrontendOptions& opts) : opts{opts}
    {}
    virtual ~Frontend() = default;
    virtual ResNone Parse() = 0;
    virtual std::vector<std::unique_ptr<Image>> GetImages() = 0;

  protected:
    FrontendOptions opts;
    // These contain all the images/partitions that have been parsed
    std::vector<std::unique_ptr<Image>> images{};
    std::vector<std::unique_ptr<Partition>> partitions{};
    // These are any references between them. They get resolved at the end of parsing
    std::vector<CompRef<Image>> imageRefs{};
    std::vector<CompRef<Partition>> partRefs{};
};

// Frontend for specifying an image on the command line
class ImageCmd : public Frontend
{
  public:
    ImageCmd() = default;
    ImageCmd (FrontendOptions& opts) : Frontend (opts)
    {}
};

#endif
