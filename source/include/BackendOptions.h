/*
    BackendOptions.h - contains backend cmdline options
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

#ifndef BACKENDOPTIONS_H
#define BACKENDOPTIONS_H

#include "include/Option.h"
#include "include/EnumArray.h"
#include "BackendTypes.h"

// libkrun backend options
static Option KrunBackendOpt[] = {
    {"",
     "-krun-vcpus",
     OptionId::KrunVcpus,
     "Specifies number of vCPUs to use for krun backend",
     true,
     "NUM"},
    {"",
     "-krun-mem",
     OptionId::KrunMem,
     "Specifies amount of memory to use for krun backend",
     true,
     "SIZE"},
    {"",
     "-krun-appliance",
     OptionId::KrunAppliance,
     "Specifies path to a krun appliance.\nIf not specified, default appliance will be "
     "used\nAppliance must be in xz-wrapped tarball",
     true,
     "FILE"},
    {.shortOpt = nullptr, .longOpt = nullptr}};

static Option LoopbackBackendOpt[] = {{.shortOpt = nullptr, .longOpt = nullptr}};

static Option GuestfsBackendOpt[] = {{.shortOpt = nullptr, .longOpt = nullptr}};

static Option XorrisoBackendOpt[] = {{.shortOpt = nullptr, .longOpt = nullptr}};

// Master backend option table
static EnumArray<BackendType, Option*, BackendType::Max> BackendOptions = {
    {BackendType::None, nullptr},    // None
    {BackendType::Krun, KrunBackendOpt},
    {BackendType::Loopback, LoopbackBackendOpt},
    {BackendType::Guestfs, GuestfsBackendOpt},
    {BackendType::Xorriso, XorrisoBackendOpt}};

#endif
