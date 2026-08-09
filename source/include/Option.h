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

#include "include/OptionTypes.h"

struct Option
{
    const char* shortOpt;
    const char* longOpt;
    OptionId id;
    const char* helpString;    // String printed out by help
    bool requiresArg;
    const char* argHelp;    // String printed out by help to describe the argument
};

struct ActionHelp
{
    const char* action;
    Option* opts;
};

#endif
