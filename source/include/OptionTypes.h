/*
    OptionTypes.h - contains all defined command line option types
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

#ifndef OPTIONTYPES_H
#define OPTIONTYPES_H

// TODO: I would love to have a way to auto generate the option table / IDs from a single table
// but this will work and any other way would require some shell trickery

// All valid options
enum class OptionId
{
    None,
    ////////////////////
    // GLOBAL OPTIONS //
    ////////////////////
    ConfFile,
    NamePrefix,
    OutputDir,
    ConfEnc,
    Backend,
    FailOnSkip,
    ////////////////////
    // ACTION OPTIONS //
    ////////////////////
    Overwrite,
    SrcDir,
    ////////////////////
    // BACKEND OPTIONS //
    ////////////////////

    // LIBKRUN BACKEND OPTIONS
    KrunVcpus,
    KrunMem,
    KrunAppliance
};

#endif
