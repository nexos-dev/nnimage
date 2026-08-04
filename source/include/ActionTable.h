/*
    ActionTable.h - contains actions
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

#ifndef ACTIONTABLE_H
#define ACTIONTABLE_H

#include "include/Action.h"

#include <functional>
#include <unordered_map>

class CreateAction : public Action
{
  public:
    CreateAction() : Action (ActionType::ActionCreate)
    {}
    Option* GetOptions()
    {
        return CreateOpt;
    }
    bool ValidateOptions();
    bool Execute();
    bool SetOption (OptionId opt, const std::string& val);

  private:
    bool overwrite = false;    // Wheter to overwrite an existing image
};

class PartitionAction : public Action
{
  public:
    PartitionAction() : Action (ActionType::ActionPartition)
    {}
    Option* GetOptions()
    {
        return PartitionOpt;
    }
    bool Execute();
    bool ValidateOptions();
    bool SetOption (OptionId opt, const std::string& val);
};

class FormatAction : public Action
{
  public:
    FormatAction() : Action (ActionType::ActionFormat)
    {}
    Option* GetOptions()
    {
        return FormatOpt;
    }
    bool Execute();
    bool ValidateOptions();
    bool SetOption (OptionId opt, const std::string& val);
};

class UpdateAction : public Action
{
  public:
    UpdateAction() : Action (ActionType::ActionUpdate)
    {}
    Option* GetOptions()
    {
        return UpdateOpt;
    }
    bool Execute();
    bool ValidateOptions();
    bool SetOption (OptionId opt, const std::string& val);

  private:
    std::string srcDir;    // Directory to get files from
};

// Action table
// clang-format off
using ActionSetter = std::function<std::unique_ptr<Action> ()>;
const static std::unordered_map<std::string, ActionSetter> actionTable = {
    {"create", [] () { return std::make_unique<CreateAction>(); }},
    {"partition", [] () { return std::make_unique<PartitionAction>(); }},
    {"format", [] () { return std::make_unique<FormatAction>();}},
    {"update", [] () { return std::make_unique<UpdateAction>(); }}
};

#endif
