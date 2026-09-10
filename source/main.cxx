/*
    main.cxx - contains entry point for nnimage
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

#include "include/Dispatch.h"
#include "include/Log.h"
#include "config.h"
#include "cxxopts.hpp"

#include <cstdlib>
#include <iostream>
#include <libgen.h>
#include <memory>
#include <string>

#ifdef NNIMAGE_ENABLE_TESTS
// Test driver function, only linked in when tests are enabled
bool TestDriver (int argc, char** argv);
#endif

// Global log instance
std::unique_ptr<Log> _log;

static bool createLog (const char* progName)
{
    _log = std::make_unique<Log>();
    // Add cout and cerr to it at their defaults
    LogSinkInfo coutSink = {LogLevel::Info, LogLevel::Status};
    auto res = _log->AddConsoleSink (coutSink, progName, std::cout);
    if (!res.IsOk())
        return false;

    LogSinkInfo cerrSink = {LogLevel::Warning, LogLevel::Fatal};
    res = _log->AddConsoleSink (cerrSink, progName, std::cerr);
    if (!res.IsOk())
        return false;
    return true;
}

static void version()
{
    std::cout << "nnimage version " << NNIMAGE_VERSION << std::endl;
    std::cout << "Copyright (C) 2026 Jedidiah Thompson" << std::endl;
    std::cout << "See https://www.apache.org/licenses/LICENSE-2.0 for licensing" << std::endl;
}

static void help (cxxopts::Options& opts)
{
    std::cout << "nnimage: " << opts.help() << std::endl;
    std::cout << "For more info, run \"man nnimage\"" << std::endl;
}

static void prepareOpts (cxxopts::Options& opts)
{
    opts.custom_help ("<operation> [-f conf_file] [-i image] [-o output] [options]");
    opts.positional_help ("\nTakes configuration found in conf_file, or configuration specified on the command "
                          "line and outputs it into specified output file.\nFor mult-image configurations, "
                          "use -i to specify which images to generate");
    opts.set_width (90);
    // clang-format off
    opts.add_options("Global")
        ("h,help", "Shows this help screen")
        ("v,version", "Shows version information");
    // clang-format on
}

static cxxopts::ParseResult parseOpts (cxxopts::Options& opts, int argc, char** argv)
{
    try
    {
        auto res = opts.parse (argc, argv);
        // Check for extra positional arguments
        if (!res.unmatched().empty())
        {
            // Only print the first one out to avoid being too verbose
            _log->Error (
                "Unexpected extra argument \"" + res.unmatched().front() + "\"\nRun " + argv[0] + " --help for usage");
            std::exit (1);
        }
        return res;
    }
    catch (const cxxopts::exceptions::exception& e)
    {
        _log->Error (std::string (e.what()) + "\nRun " + argv[0] + " --help for usage");
        std::exit (1);
    }
}

int main (int argc, char** argv)
{
    // First task we have is to create the initial log
    if (!createLog (argv[0]))
    {
        std::cerr << argv[0] << ": error: Failed to create log" << std::endl;
        return 1;
    }

    // Now see if we want to call test driver
#ifdef NNIMAGE_ENABLE_TESTS
    if (argc > 1 && std::string (argv[1]) == "run-test-cases")
        return !TestDriver (argc - 1, argv + 1);
#endif

    Dispatch disp;

    // Now we need to prepare the command line
    cxxopts::Options opts (basename (argv[0]), "A powerful, easy-to-use, all-in-one disk image manager");
    prepareOpts (opts);
    disp.CollectOptions (opts);
    auto result = parseOpts (opts, argc, argv);

    // Now check for help/version
    if (result.count ("help"))
    {
        help (opts);
        return 0;
    }
    else if (result.count ("version"))
    {
        version();
        return 0;
    }

    // Prepare for the dispatcher to run
    auto res = disp.ValidateOptions();
    if (!res.IsOk())
    {
        _log->Error (res.GetError().RootFrame().msg + std::string ("\nRun ") + argv[0] + " --help for usage");
        return 1;
    }

    // We have all the info we need now, begin the dispatcher
    return !disp.Execute();
}
