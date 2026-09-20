/*
    OptionParser.h - contains options parser class
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

#ifndef OPTIONPARSER_H
#define OPTIONPARSER_H

#include "include/Error.h"
#include "include/Log.h"
#include "include/StringHash.h"

#include <memory>
#include <vector>
#include <string>
#include <concepts>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <cassert>

// NOTE: this is here to prevent cxxopts from comma-splitting
struct CommaString
{
    std::vector<std::string> values;
};

// Option tracker class
class OptionTracker
{
  public:
    void MapOption (std::string opt, std::string set)
    {
        assert (map.find (opt) == map.end());
        map[std::move (opt)] = std::move (set);
    }
    void UseSet (std::string set)
    {
        usedSets.insert (std::move (set));
    }
    bool IsOptionUsed (std::string_view option)
    {
        auto it = map.find (option);
        assert (it != map.end());

        const std::string& set = it->second;
        return usedSets.contains (set);
    }

  private:
    // Maps an option to it's set
    std::unordered_map<std::string, std::string, StringHash, std::equal_to<>> map;
    // List of option sets that have been used
    std::unordered_set<std::string, StringHash, std::equal_to<>> usedSets;
};

class OptionAdder;

enum class OptionsResult
{
    Normal,
    Error,
    ExitSuccess    // Used if --help or --version is passed
};

class OptionsParser
{
  public:
    OptionsParser (std::string_view progName, int argc, const char* const* argv);
    ~OptionsParser();

    OptionsParser (const OptionsParser&) = delete;
    OptionsParser& operator= (const OptionsParser&) = delete;
    OptionsParser (OptionsParser&&) noexcept;
    OptionsParser& operator= (OptionsParser&&) noexcept;

    OptionsResult Parse();
    OptionAdder AddOptions (std::string_view helpGroup, std::string trackSet);
    void AddPositional (std::string_view option);

    void UseSet (std::string set)
    {
        track.UseSet (std::move (set));
    }

    void OptError (std::string_view msg)
    {
        Log::The().Error (std::format ("{}\nRun \"{}\" --help for usage", msg, argv[0]));
    }

    ResNone CheckUnusedOpts();

  private:
    void prepareHelp();
    void help();
    void version();

    OptionTracker track;
    // cxxopts PIMPL
    struct OptionParseImpl;
    std::unique_ptr<OptionParseImpl> impl;
    int argc;
    const char* const* argv;

    // Pre-defined strings
    // FIXME: change to string_view
    inline static const std::string progHelp = "An all-in-one, powerful, easy to use disk image manager";
    inline static const std::string usage = "<operation> [-f conf_file] [-i image] [-o output] [options]";
    inline static const std::string explanation =
        "\nTakes configuration found in conf_file, or configuration specified on the command "
        "line and outputs it into specified output file.\nFor mult-image configurations, "
        "use -i to specify which images to generate";

    static constexpr int helpWidth = 100;

    friend class OptionAdder;    // Option adder needs impl access
};

// Concept of allowed argument values
template <typename T>
concept AllowedArgVal = std::same_as<T, std::string> || std::same_as<T, int> || std::same_as<T, bool> ||
                        std::same_as<T, CommaString> || std::same_as<T, std::vector<std::string>>;

class OptionAdder
{
  public:
    OptionAdder (OptionsParser& parser, OptionTracker& tracker, std::string_view helpGroup, std::string set);
    ~OptionAdder();

    OptionAdder (const OptionAdder&) = delete;
    OptionAdder& operator= (const OptionAdder&) = delete;
    OptionAdder (OptionAdder&&) noexcept;
    OptionAdder& operator= (OptionAdder&&) noexcept;

    template <AllowedArgVal V>
    OptionAdder& operator() (std::string_view args, std::string_view desc, V& out, std::string_view help = "");

  private:
    // PIMPL for cxxopts internals
    struct OptionAddImpl;
    std::unique_ptr<OptionAddImpl> impl;

    std::string set;

    OptionTracker& tracker;
    OptionsParser& parser;
};

#endif
