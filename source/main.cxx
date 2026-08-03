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

#include "nnimage.h"
#include <iostream>

#include "Option.h"

// Global log instance
std::unique_ptr<Log> _log;
std::unique_ptr<CmdLine> _cmdLine;

// Initialize command line
CmdLine::CmdLine (int argc, char** argv)
{
    // Initialize arguments
    rawArgs.reserve (argc - 1);
    for (int i = 1; i < argc; ++i)
        rawArgs.push_back (argv[i]);
}

// Finds option in option array
const Option* CmdLine::findOptionInArray (const std::string& opt, const Option* opts)
{
    int i = 0;
    for (;;)
    {
        const Option* iter = &opts[i++];
        if (!iter->shortOpt && !iter->longOpt)
            return nullptr;
        if (iter->shortOpt == opt || iter->longOpt == opt)
            return iter;
    }
    return nullptr;
}

bool CmdLine::ParseArguments (HelpCb helpCallback, VersionCb versionCallback)
{
    // Find an action
    if (rawArgs.size() >= 1)
    {
        if (!rawArgs[0].starts_with ("-"))
        {
            auto& actionName = rawArgs[0];
            action = Action::MakeAction (actionName);
            if (action == nullptr)
            {
                _log->Error ("invalid action \"" + actionName + "\"");
                return false;    // This means the action was invalid
            }
            rawArgs.erase (rawArgs.begin() + 0);
        }
    }
    // Loop through every argument
    for (int i = 0; i < rawArgs.size(); i++)
    {
        const std::string& optName = rawArgs[i];
        // Make sure this is an option
        if (!optName.starts_with ("-"))
        {
            _log->Error ("invalid argument \"" + optName + "\"");
            return false;
        }
        // Handle special cases
        if (optName == "-h" || optName == "-help")
            helpCallback();
        else if (optName == "-v" || optName == "-version")
            versionCallback();
        // Now we have to find this option in either GlobalOpts or the action's options
        const Option* opt = findOptionInArray (optName, GlobalOpt);
        if (opt == nullptr && action)
            opt = findOptionInArray (optName, action->GetOptions());
        if (opt == nullptr)
        {
            _log->Error ("invalid option \"" + optName + "\"");
            return false;
        }
        // Ok, so have the option now. Now we have to determine wheter this option takes an argument
        std::string value;
        if (opt->requiresArg)
        {
            // Make sure there's an argument
            if (i + 1 == rawArgs.size() || rawArgs[i + 1].starts_with ("-"))
            {
                // This is an option, that's an error
                _log->Error ("option \"" + optName + "\" requires an argument");
                return false;
            }
            value = std::move (rawArgs[++i]);
        }
        // Set it if the action is valid
        if (!action)
        {
            _log->Error ("argument \"" + optName + "\" requires an action");
            return false;
        }
        if (!action->SetOption (opt->id, value))
            return false;
    }
    // Make sure we have an action
    if (!action)
    {
        _log->Error ("action not specified.\nRun \"nnimage -h\" for help");
        return false;
    }
    // Now validate it
    if (!action->ValidateOptions())
        return false;
    return true;
}

static inline void printOptHelp (const Option& opt)
{
    std::cout << "  " << opt.shortOpt;
    if (opt.requiresArg && opt.argHelp)
        std::cout << " " << opt.argHelp << "\n";
    else
        std::cout << "\n";
    // Split up help string
    size_t pos = 0;
    size_t lastPos = 0;
    std::string cur;
    const std::string& str = opt.helpString;
    while ((pos = str.find ("\n", lastPos)) != std::string::npos)
    {
        cur = str.substr (lastPos, pos - lastPos);
        lastPos = pos + 1;
        std::cout << "        " << cur << "\n";
    }
    // Print last part of string
    cur = str.substr (lastPos);
    std::cout << "        " << cur << "\n";
}

static void Help()
{
    std::cout << "nnimage - disk image management helper\n"
              << "nnimage allows you to manage the contents of a disk image for things like\n"
              << "an OS distribution in a simple, fast and efficient way.\n"
              << "Does not require root privileges to run, (except with the loopback backend), \n"
              << "and can be used in a build system to create images\n"
              << "Usage: nnimage [-h|-v] ACTION [-f FILE] [-i IMAGE] OPTIONS\n"
                 "Arguments:\n";
    // Now document every argument
    for (const Option& opt : GlobalOpt)
    {
        // Print option
        if (opt.helpString)
            printOptHelp (opt);
    }
    // Now add on -h and -v
    std::cout << "  -h\n";
    std::cout << "        Prints this page\n";
    std::cout << "  -v\n";
    std::cout << "        Shows version info\n";
    // Now show for every action
    for (const auto& action : Actions)
    {
        if (action.opts)
        {
            std::cout << "\nOptions for action \"" << action.action << "\":\n";
            size_t i = 0;
            while (1)
            {
                const Option& opt = action.opts[i];
                // Check if this is the end
                if (!opt.shortOpt && !opt.longOpt)
                    break;
                // Print option
                if (opt.helpString)
                    printOptHelp (opt);
                ++i;
            }
        }
    }
    std::cout << "Reads specified configuration file, and performs action into specified image\n"
              << "See nnimage(1) for full documentation.\n";
    std::exit (0);
}

static void Version()
{
    std::cout << "nnimage version " << NNIMAGE_VERSION << "\n";
    std::cout << "Copyright (C) 2026 Jedidiah Thompson"
              << "\n";
    std::cout << "See https://www.apache.org/licenses/LICENSE-2.0 for licensing"
              << "\n";
    std::exit (0);
}

int main (int argc, char** argv)
{
    // First see if we want to call test driver
#ifdef NNIMAGE_ENABLE_TESTS
    if (argc > 1 && std::string (argv[1]) == "run-test-cases")
        return !TestDriver (argc - 1, argv + 1);
#endif
    // Initialize command line
    _cmdLine = std::make_unique<CmdLine> (argc, argv);
    // Start up log
    _log = std::make_unique<Log> (argv[0], DEFAULT_LOGLEVEL);
    // Parse command line
    if (!_cmdLine->ParseArguments (Help, Version))
        return 1;
    // OK so we now have the arguments. Now it's time to parse the configuration file
    // First get the action
    Action* act = _cmdLine->GetAction();
    // Now start the configuration
    ImageConf conf = ImageConf (act->GetConf());
    if (!conf.ParseFile())
    {
        // Configuration parsing failed
        return 1;
    }
    // Now execute the action and return
    return !act->Execute();
}
