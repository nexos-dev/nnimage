/*
    CmdLine.h - command line declarations
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

#ifndef NNIMAGE_CMDLINE_H
#define NNIMAGE_CMDLINE_H

#include "include/Action.h"

#include <memory>
#include <string>
#include <vector>

typedef void (*HelpCb)();
typedef void (*VersionCb)();

class CmdLine
{
  public:
    CmdLine() = delete;
    CmdLine (int argc, char** argv);
    bool ParseArguments (HelpCb helpCallback, VersionCb versionCallback);
    Action* GetAction()
    {
        return action.get();
    }

  private:
    std::vector<std::string> rawArgs;
    std::unique_ptr<Action> action;
    const Option* findOptionInArray (const std::string& opt, const Option* opts);
};

extern std::unique_ptr<CmdLine> _cmdLine;

static inline Action* GetAction()
{
    if (_actionOverride)
        return _actionOverride;
    return _cmdLine->GetAction();
}

#endif
