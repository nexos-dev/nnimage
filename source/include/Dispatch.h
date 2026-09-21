/*
    Dispatch.h - contains main dispatcher header
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

#ifndef DISPATCH_H
#define DISPATCH_H

#include "include/Action.h"
#include "include/Error.h"
#include "include/EnumArray.h"
#include "include/Frontend.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct ActionReg
{
    std::unique_ptr<Action> action;
    std::unique_ptr<ActionOptions> options;
};

// The raw options
struct DispatchOptions
{
    // Verbosity options
    bool quiet = false;
    bool verbose = false;
    bool traceErrors = false;
    std::string defaultBackend = "";
    std::string operation = "";
    std::vector<std::string> logFiles{};
};

class Dispatch
{
  public:
    Dispatch();
    void CollectOptions (OptionsParser& opts);
    ResNone ValidateOptions();
    ResNone SetupConf();
    bool Execute (OptionsParser& parser);

  private:
    ResNone setupLogs();
    ResNone setupError();
    Result<std::filesystem::path> getLogDir();

    std::filesystem::path getConfigDir()
    {
        // NOTE: maybe we should allow for a system wide config, but I like per user config better
        const char* home = getenv ("HOME");
        assert (home);
        std::filesystem::path dir = std::filesystem::path (home) / ".config" / "nnimage";
        return dir;
    }
    // This function is the end of the line for most errors that occur in this program
    void dispatchFail (Error& err)
    {
        err = err.Chain ({ErrorDomain::Operation, ErrorCode::OpFailed, ErrorLog::Normal, ErrorSeverity::Fatal}, {});
        ErrorOutput::The()->Report (err);
    }
    // Helper for reporting option validation errors
    Error makeOptionError (std::string_view msg)
    {
        return Error ({ErrorDomain::Option, ErrorCode::InvalidOption}, {{"message", std::string (msg)}});
    }

    std::filesystem::path logDir;
    std::filesystem::path confFile;

    EnumArray<ActionType, ActionReg, ActionType::Max> actionTable;
    DispatchOptions options;

    FrontendOptions frontOpts;
};

#endif
