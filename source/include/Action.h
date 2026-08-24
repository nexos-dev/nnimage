/*
    Action.h - action declarations
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

#ifndef NNIMAGE_ACTION_H
#define NNIMAGE_ACTION_H

#include "include/Options.h"
#include "include/EnumArray.h"
#include "include/Error.h"

enum class ActionType
{
    Create,
    Init,
    Partition,
    Format,
    Update,
    Max
};

class Action
{
  public:
    Action() = delete;
    virtual ~Action() = default;
    Action (ActionType action) : type{action}
    {}

    static Result<ActionType> ResolveName (std::string name)
    {
        auto it = actionNameTable.find (name);
        if (it == actionNameTable.end())
            return Error ({ErrorDomain::Action, ErrorCode::BadAction}, "invalid action name specfied");
        return it->second;
    }

    Action (const Action&) = delete;
    Action& operator= (const Action&) = delete;

  protected:
    ActionType type;

  private:
    inline static const std::unordered_map<std::string, ActionType> actionNameTable = {
        {"create", ActionType::Create},
        {"init", ActionType::Init},
        {"partition", ActionType::Partition},
        {"format", ActionType::Format},
        {"update", ActionType::Update}};
};

class ActionOptions;
using ActOptSetter = std::function<std::unique_ptr<ActionOptions>()>;

class ActionOptions : public Options
{
  public:
    virtual ~ActionOptions() = default;
    virtual std::unique_ptr<Action> MakeAction() = 0;
    static std::unique_ptr<ActionOptions> MakeActOptions (ActionType type);

  private:
    const static EnumArray<ActionType, ActOptSetter, ActionType::Max> actionTable;
};

#endif
