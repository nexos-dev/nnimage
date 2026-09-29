/*
    Frontend.cxx - contains general frontend logic
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

#include "include/Frontend.h"
#include <string>

void FrontendOptions::CollectOptions (OptionsParser& opts)
{
    // clang-format off
    opts.AddOptions ("Frontend", "frontend_cmd")
        ("s,size", "Specifies size of image.\nMust be suffixed with multiplier (e.g., B, MiB, GB, etc)", imgSize)
        ("t,type", "Specifies image partition type (mbr, gpt, iso9660, or floppy)", imgType)
        ("bootmode", "Specifies boot mode of image (bios, efi, none)", bootMode)
        ("imgprop", "Specifies an additional property of image.\n"
            "Any property valid in configuration file is valid here", props)
        ("p,partition", "Specifies a partition to add", partSpecs);
    opts.AddOptions ("Frontend", "frontend_conf")
        ("f,file", "Configuration to get desired image configurations from", confFile)
        ("confenc", "Encoding to use for configuration file", confEnc);
    // clang-format on
}

ResNone FrontendOptions::ValidateOptions()
{
    bool hasConfFile = !confFile.empty();
    bool hasImgSize = !imgSize.empty();
    bool hasPartitions = !partSpecs.values.empty();
    bool hasImgSpec = hasPartitions || hasImgSize;

    if (hasConfFile)
    {
        if (hasImgSpec)
            return makeOptionError ("Only one of configuration file or image specification can be provided");

        return Success();
    }

    return Success();
}

std::unique_ptr<Frontend> FrontendOptions::CreateFrontend (OptionsParser& parser)
{
    // If a configuration file was provided, this is a ImageConf instance.
    // Other wise an ImageCmd
    if (!confFile.empty())
    {
        parser.UseSet ("frontend_conf");
        return std::make_unique<ImageConf> (*this);
    }
    parser.UseSet ("frontend_cmd");
    return std::make_unique<ImageCmd> (*this);
}

std::vector<std::reference_wrapper<Image>> ImageSet::GetImages() const
{
    std::vector<std::reference_wrapper<Image>> vec;
    vec.reserve (images.size());
    for (auto& [key, ptr] : images)
        vec.push_back (*ptr);
    return vec;
}

std::optional<std::reference_wrapper<Image>> ImageSet::FindImage (std::string_view name) const
{
    auto it = images.find (name);
    if (it == images.end())
        return std::nullopt;
    return *it->second;
}

std::optional<std::reference_wrapper<Partition>> ImageSet::FindPartition (std::string_view name) const
{
    auto it = partitions.find (name);
    if (it == partitions.end())
        return std::nullopt;
    return *it->second;
}

std::shared_ptr<Partition> ImageSet::FindPartitionShared (std::string_view name) const
{
    auto it = partitions.find (name);
    if (it == partitions.end())
        return {};
    return it->second;
}

ResNone ImageSet::AddImage (std::unique_ptr<Image> image)
{
    if (image == nullptr)
        throw std::invalid_argument ("Image can't be null");

    if (images.find (image->GetName()) != images.end())
    {
        return ImageError::Make (ErrorCode::DuplicateImage,
            {{"name_suffix", ImageError::NameSuffix (image->GetName())}});
    }

    images.emplace (image->GetName(), std::move (image));
    return Success();
}

ResNone ImageSet::AddPartition (std::shared_ptr<Partition> part)
{
    if (part == nullptr)
        throw std::invalid_argument ("Partition can't be null");

    if (partitions.find (part->GetName()) != partitions.end())
    {
        return ImageError::Make (ErrorCode::DuplicatePartition,
            {{"name_suffix", ImageError::NameSuffix (part->GetName())}});
    }
    partitions.emplace (part->GetName(), std::move (part));
    return Success();
}
