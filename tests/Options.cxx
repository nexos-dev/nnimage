/*
    Options.cxx - contains OptionsParser test cases
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

#include "doctest.h"
#include "include/OptionParser.h"

#include <string>
#include <vector>

TEST_CASE ("OptionsParser parses registered values and positional arguments")
{
    const char* argv[] =
        {"nnimage", "create", "--quiet", "--backend", "loopback", "--log-file", "one.log", "--log-file", "two.log"};
    bool quiet = false;
    std::string backend;
    std::string operation;
    std::vector<std::string> logFiles;
    OptionsParser parser ("nnimage", static_cast<int> (std::size (argv)), argv);
    // clang-format off
    parser.AddOptions ("Test", "test")
        ("operation", "Operation to perform", operation)
        ("q,quiet", "Quiet mode", quiet)
        ("b,backend", "Backend", backend)
        ("l,log-file", "Log file", logFiles);
    // clang-format on
    parser.AddPositional ("operation");

    CHECK (parser.Parse() == OptionsResult::Normal);
    CHECK (operation == "create");
    CHECK (backend == "loopback");
    CHECK (quiet);
    CHECK (logFiles == std::vector<std::string>{"one.log", "two.log"});
}

TEST_CASE ("OptionsParser preserves commas in CommaString values")
{
    const char* argv[] = {"nnimage", "--partition", "size=10MiB,type=linux"};
    CommaString partition;
    OptionsParser parser ("nnimage", static_cast<int> (std::size (argv)), argv);
    parser.AddOptions ("Test", "test") ("p,partition", "Partition specification", partition);

    CHECK (parser.Parse() == OptionsResult::Normal);
    REQUIRE (partition.values.size() == 1);
    CHECK (partition.values.front() == "size=10MiB,type=linux");
}

TEST_CASE ("OptionsParser rejects unexpected positional arguments")
{
    const char* argv[] = {"nnimage", "create", "extra"};
    std::string operation;
    OptionsParser parser ("nnimage", static_cast<int> (std::size (argv)), argv);
    parser.AddOptions ("Test", "test") ("operation", "Operation to perform", operation);
    parser.AddPositional ("operation");

    CHECK (parser.Parse() == OptionsResult::Error);
}

TEST_CASE ("OptionsParser returns an exit result for help and version")
{
    const char* helpArgv[] = {"nnimage", "--help"};
    OptionsParser helpParser ("nnimage", static_cast<int> (std::size (helpArgv)), helpArgv);
    CHECK (helpParser.Parse() == OptionsResult::ExitSuccess);

    const char* versionArgv[] = {"nnimage", "--version"};
    OptionsParser versionParser ("nnimage", static_cast<int> (std::size (versionArgv)), versionArgv);
    CHECK (versionParser.Parse() == OptionsResult::ExitSuccess);
}
