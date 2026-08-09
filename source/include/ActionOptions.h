/*
    ActionOptions.h - contains option tables for actions
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

#ifndef ACTIONOPTIONS_H
#define ACTIONOPTIONS_H

#include "include/Option.h"

// TODO: I might want to add this at the class level for each action, but for now this is fine

static Option GlobalOpt[] = {
    {"-f",
     "-conf",
     OptionId::ConfFile,
     "Specifies configuration file to read from.\nDefaults to \"nnimage.conf\"",
     true,
     "FILE"},
    {"-n",
     "-nameprefix",
     OptionId::NamePrefix,
     "Specifies file name prefix for output.\nOutput image will have image name and an appropriate extension added.\n\
If an extension is specified, then argument will be whole image name",
     true,
     "IMAGE"},
    {"-o",
     "-output",
     OptionId::OutputDir,
     "Specifies output directory for image files.\nDefaults to current working directory",
     true,
     "OUTPUT"},
    {"-e",
     "-confenc",
     OptionId::ConfEnc,
     "Specifies character encoding for configuration file.\nMust be passable to iconv(3)",
     true,
     "ENC"},
    {"-b", "-backend", OptionId::Backend, nullptr, true, nullptr},
    {"",
     "-fail-on-skip",
     OptionId::FailOnSkip,
     "Specifies to fail the action if any image is skipped due to validation errors",
     false,
     nullptr},
    {.shortOpt = nullptr, .longOpt = nullptr}};

// Create image options
static Option CreateOpt[] = {
    {"-w", "-overwrite", OptionId::Overwrite, "Specifies to overwrite pre-existing image", false},
    {nullptr}};
// Partition options
static Option PartitionOpt[] = {{nullptr}};
// FOrmat options
static Option FormatOpt[] = {{nullptr}};
// Update options
static Option UpdateOpt[] = {{"-d",
                              "-directory",
                              OptionId::SrcDir,
                              "Specifies directory to use as input for updating",
                              true,
                              "DIR"},
                             {nullptr}};

// Table to start action option help
static ActionHelp Actions[] = {{"create", CreateOpt},
                               {"partition", nullptr},
                               {"format", nullptr},
                               {"update", UpdateOpt}};

#endif
