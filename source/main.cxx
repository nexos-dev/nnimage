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
#include "include/OptionParser.h"
#include "config.h"

#include <cstdlib>
#include <iostream>
#include <libgen.h>
#include <memory>
#include <string>

static bool createLog (const char* progName)
{
    // Add cout and cerr to it at their defaults
    LogSinkInfo coutSink = {LogLevel::Info, LogLevel::Status};
    auto res = Log::The().AddConsoleSink (coutSink, progName, std::cout);
    if (!res)
        return false;

    LogSinkInfo cerrSink = {LogLevel::Warning, LogLevel::Fatal};
    res = Log::The().AddConsoleSink (cerrSink, progName, std::cerr);
    if (!res)
        return false;
    return true;
}

int main (int argc, char** argv)
{
    // First task we have is to create the initial log
    if (!createLog (argv[0]))
    {
        std::cerr << argv[0] << ": error: Failed to create log" << std::endl;
        return 1;
    }

    Dispatch disp;
    OptionsParser opts (basename (argv[0]), argc, argv);

    disp.CollectOptions (opts);
    auto result = opts.Parse();

    if (result == OptionsResult::ExitSuccess)
        return 0;
    else if (result == OptionsResult::Error)
        return 1;

    // Prepare for the dispatcher to run
    auto res = disp.ValidateOptions();
    if (!res)
    {
        opts.OptError (res.Error().RootFrame().msg);
        return 1;
    }

    // We have all the info we need now, begin the dispatcher
    return !disp.Execute (opts);
}
