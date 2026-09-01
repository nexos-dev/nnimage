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

#include "nnimage.h"

// NOTE: cxxopts splits options into a vector by comma by default. However, we dont want that.
// This is because a partition spec is supposed a set of comma-seperated key=value pairs.
// To prevent, we define a custom type that will contain an array and use this operator to parse into it
std::istream& operator>> (std::istream& is, PartitionStrings& val)
{
    std::string spec;
    is >> spec;
    val.values.push_back (spec);
    return is;
}

void FrontendOptions::CollectOptions (cxxopts::Options& opts)
{
    // clang-format off
    opts.add_options("Frontend")
        ("f,file", "Configuration to get desired image configurations from", 
            cxxopts::value<std::string>(confFile), "CONF_FILE")
        ("confenc", "Encoding to use for configuration file", 
            cxxopts::value<std::string>(confEnc), "ENC")
        ("s,size", "Specifies size of image.\nMust be suffixed with multiplier (e.g., B, MiB, GB, etc)", 
            cxxopts::value<std::string>(imgSize), "SIZE")
        ("bootmode", "Specifies boot mode of image (bios, efi, none)", 
            cxxopts::value<std::string>(bootMode), "MODE")
        ("imgprop", "Specifies an additional property of image.\n"
            "Any property valid in configuration file is valid here",
            cxxopts::value<std::vector<std::string>>(props), "PROP=VALUE...")
        ("p,partition", "Specifies a partition to add", cxxopts::value<PartitionStrings>(partSpecs), "PART=SPEC...");
    // clang-format on
}

ResNone FrontendOptions::ValidateOptions()
{
    return Success();
}

std::unique_ptr<Frontend> FrontendOptions::CreateFrontend()
{
    return nullptr;
}
