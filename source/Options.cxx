/*
    Options.cxx - contains cxxopts wrapper for catching unused options
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

#include "include/OptionParser.h"
#include "cxxopts.hpp"

#include <iostream>
#include <istream>

// To make CommaString parsable by cxxopts
std::istream& operator>> (std::istream& is, CommaString& val)
{
    std::string spec;
    is >> spec;
    val.values.push_back (spec);
    return is;
}

// Implementations

struct OptionAdder::OptionAddImpl
{
    OptionAddImpl (cxxopts::Options& opts, std::string_view helpGroup) : adder{opts, std::string (helpGroup)}
    {}
    cxxopts::OptionAdder adder;
};

struct OptionsParser::OptionParseImpl
{
    OptionParseImpl (std::string_view progName, std::string_view help)
        : opts (std::string (progName), std::string (help))
    {}

    cxxopts::Options opts;
    cxxopts::ParseResult result;
};

OptionAdder::OptionAdder (OptionsParser& parser, OptionTracker& tracker, std::string_view helpGroup, std::string set)
    : tracker{tracker}, parser{parser}, set{std::move (set)}
{
    impl = std::make_unique<OptionAddImpl> (parser.impl->opts, helpGroup);
}

OptionAdder::~OptionAdder() = default;

OptionAdder::OptionAdder (OptionAdder&&) noexcept = default;

OptionAdder& OptionAdder::operator= (OptionAdder&& other) noexcept
{
    if (this != &other)
    {
        impl = std::move (other.impl);
        set = std::move (other.set);
    }
    return *this;
}

template <AllowedArgVal V>
OptionAdder& OptionAdder::operator() (std::string_view args, std::string_view desc, V& out, std::string_view help)
{
    impl->adder (std::string (args), std::string (desc), cxxopts::value<V> (out), std::string (help));

    // Get long option from args
    // First split short/long option
    size_t pos = args.find (',');
    std::string longOpt;
    if (pos == std::string_view::npos)
        longOpt = args;    // It's just a long option
    else
        longOpt = args.substr (pos + 1);

    tracker.MapOption (std::move (longOpt), set);
    return *this;
}

OptionsParser::OptionsParser (std::string_view progName, int argc, const char* const* argv) : argc{argc}, argv{argv}
{
    impl = std::make_unique<OptionParseImpl> (progName, progHelp);
}

OptionsParser::~OptionsParser() = default;

OptionsParser::OptionsParser (OptionsParser&&) noexcept = default;

OptionsParser& OptionsParser::operator= (OptionsParser&&) noexcept = default;

OptionAdder OptionsParser::AddOptions (std::string_view helpGroup, std::string trackSet)
{
    return OptionAdder (*this, track, helpGroup, std::move (trackSet));
}

OptionsResult OptionsParser::Parse()
{
    prepareHelp();

    cxxopts::Options& opts = impl->opts;
    try
    {
        impl->result = opts.parse (argc, argv);
        // Check for extra positional arguments
        auto& unmatched = impl->result.unmatched();
        if (!unmatched.empty())
        {
            // Only print the first one out to avoid being too verbose
            OptError ("Unexpected extra argument \"" + unmatched.front());
            return OptionsResult::Error;
        }
    }
    catch (const cxxopts::exceptions::exception& e)
    {
        OptError (e.what());
        return OptionsResult::Error;
    }

    // Now check for help and version
    auto& result = impl->result;
    if (result.count ("help"))
    {
        help();
        return OptionsResult::ExitSuccess;
    }
    else if (result.count ("version"))
    {
        version();
        return OptionsResult::ExitSuccess;
    }

    return OptionsResult::Normal;
}

void OptionsParser::AddPositional (std::string_view option)
{
    impl->opts.parse_positional ({std::string (option)});
}

ResNone OptionsParser::CheckUnusedOpts()
{
    // Go through all the found arguments, and check the tracker to see if they were ever used
    auto& result = impl->result;

    const auto& args = result.arguments();
    for (const auto& arg : args)
    {
        const auto& argName = arg.key();
        if (!track.IsOptionUsed (argName))
            return Error ({ErrorDomain::Option, ErrorCode::UnusedArg}, {{"option", argName}});
    }
    return Success();
}

void OptionsParser::help()
{
    std::cout << "nnimage: " << impl->opts.help() << "\n";
    std::cout << "For more info, run \"man nnimage\"" << std::endl;
}

void OptionsParser::version()
{
    std::cout << "nnimage version " << NNIMAGE_VERSION << "\n";
    std::cout << "Copyright (C) 2026 Jedidiah Thompson" << "\n";
    std::cout << "See https://www.apache.org/licenses/LICENSE-2.0 for licensing" << std::endl;
    std::cout << "\nContains code from:\n";
    std::cout << "  - cxxopts: (C) Jarryd Beck, MIT License\n";
    std::cout << "  - doctest: (C) Viktor Kirilov, MIT License\n";
    std::cout << "  - MemoryMapped: (C) Stephen Brumme, zlib license" << std::endl;
}

void OptionsParser::prepareHelp()
{
    cxxopts::Options& opts = impl->opts;
    opts.custom_help (std::string (usage));
    opts.positional_help (std::string (explanation));
    opts.set_width (helpWidth);

    // clang-format off
    opts.add_options("Global")
        ("h,help", "Shows this help screen")
        ("v,version", "Shows version information");
    // clang-format on
}

// Macro to define a template for operator()

#define MAKE_ADDER(_T_) \
    template OptionAdder& OptionAdder::operator()<_T_> (std::string_view, std::string_view, _T_&, std::string_view);

MAKE_ADDER (std::string);
MAKE_ADDER (int);
MAKE_ADDER (bool);
MAKE_ADDER (std::vector<std::string>);
MAKE_ADDER (CommaString);

#undef MAKE_ADDER
