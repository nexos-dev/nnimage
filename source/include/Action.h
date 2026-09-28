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

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

enum class ActionType
{
    Init,
    Partition,
    Format,
    Update,
    Max
};

class Image;
class Target;

class Action
{
  public:
    Action() = delete;
    virtual ~Action() = default;
    Action (ActionType action) : type{action}
    {}

    Action (const Action&) = delete;
    Action& operator= (const Action&) = delete;

    ActionType GetType() const
    {
        return type;
    }

    // Creates target list for action
    virtual ResNone FillTarget (Target& target, const Image& image) = 0;

  protected:
    ActionType type;

  private:
};

class ActionOptions;
using ActOptSetter = std::function<std::unique_ptr<ActionOptions>()>;

class ActionOptions : public Options
{
  public:
    virtual ~ActionOptions() = default;
    virtual std::unique_ptr<Action> MakeAction() = 0;
    static std::unique_ptr<ActionOptions> MakeActOptions (ActionType type);
    virtual void CollectOptions (OptionsParser& opts);

  private:
    const static EnumArray<ActionType, ActOptSetter, ActionType::Max> actionTable;
};

// CRTP base that implements MakeAction() for a concrete Action subclass
template <typename ActionT>
class ActionOptionsFactory : public ActionOptions
{
  public:
    std::unique_ptr<Action> MakeAction() override
    {
        return std::make_unique<ActionT>();
    }
};

#endif
