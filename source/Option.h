/*
    Option.h - contains all defined command line options
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

#ifndef OPTION_H
#define OPTION_H

// General options
static Option GlobalOpt[] = {
    {"-f",
     "-conf",
     "conf_file",
     "Specifies configuration file to read from.\nDefaults to \"nnimage.conf\"",
     true,
     "FILE"},
    {"-i",
     "-image",
     "image_file",
     "Specifies file name prefix for output.\nOutput image will have image name and an appropriate extension added.\n\
If an extension is specified, then argument will be whole image name",
     true,
     "IMAGE"},
    {"-e",
     "-confenc",
     "conf_enc",
     "Specifies character encoding for configuration file.\nMust be passable to iconv(3)",
     true,
     "ENC"},
    {"-b", "-backend", "default_backend", nullptr, true, nullptr},
    {.shortOpt = nullptr, .longOpt = nullptr}};

// Create image options
static Option CreateOpt[] = {
    {"-w", "-overwrite", "overwrite", "Specifies to overwrite pre-existing image", false},
    {nullptr}};
// Partition options
static Option PartitionOpt[] = {{nullptr}};
// FOrmat options
static Option FormatOpt[] = {{nullptr}};
// Update options
static Option UpdateOpt[] = {{"-d",
                              "-directory",
                              "src_directory",
                              "Specifies directory to use as input for updating",
                              true,
                              "DIR"},
                             {nullptr}};

// Action structure (for help)
struct ActionHelp
{
    const char* action;    // Name of action
    Option* opts;          // Array of options
};

static ActionHelp Actions[] = {{"create", CreateOpt},
                               {"partition", nullptr},
                               {"format", nullptr},
                               {"update", UpdateOpt}};

#endif
