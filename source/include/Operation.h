/*
    Operation.h - contains operation header
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

#ifndef OPERATION_H
#define OPERATION_H

#include "include/Image.h"
#include "include/Task.h"
#include "include/Action.h"
#include "include/EnumBitSet.h"
#include "include/OptionParser.h"
#include "include/NameRegistry.h"

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

class Operation;
class OperationOptions;

// All defined operation types
enum class OpType
{
    OpMeta,
    OpAction
};

enum class OpFlags
{
    CreatesImage,
    Max
};

// Builds a concrete Operation subclass. Only used by meta-operations
using OpFactory = std::function<std::unique_ptr<Operation> (OperationOptions&)>;

// Structure describing an operation, as found in the opTable
struct OpInfo
{
    OpType type;
    EnumBitSet<OpFlags, OpFlags::Max> flags;
    std::vector<ActionType> actions;    // fixed action sequence, used directly when type == OpAction
    OpFactory factory = nullptr;        // required when type == OpMeta, unused otherwise
};

class OperationOptions
{
  public:
    OperationOptions();
    void CollectOptions (OptionsParser& opts);
    ResNone ValidateOptions();

    // Builds an action from specified type
    std::unique_ptr<Action> MakeAction (ActionType type)
    {
        return actOptions[type]->MakeAction();
    }

  private:
    EnumArray<ActionType, std::unique_ptr<ActionOptions>, ActionType::Max> actOptions;
};

class Operation
{
  public:
    virtual ~Operation() = default;

    // Main operation function. Takes a list of images, gives back a list of targets to execute
    Result<std::vector<Target>> PrepareTargets (const std::vector<std::reference_wrapper<Image>>& images);

    static Result<std::unique_ptr<Operation>> MakeOperation (std::string_view opName, OperationOptions& opOptions);

    bool CheckFlag (OpFlags flag)
    {
        return flags.test (flag);
    }

  protected:
    explicit Operation (OperationOptions& opOptions, std::vector<ActionType> actions = {})
        : opOptions{opOptions}, actions{std::move (actions)}
    {}

    // Picks the action sequence to run against a single image. Meta-operations override this.
    virtual Result<std::vector<ActionType>> SelectActions (const Image& image)
    {
        return actions;
    }

    OperationOptions& opOptions;

  private:
    // Turns an ordered action sequence into a single Target, in order, one task per action
    Result<Target> buildTarget (const Image& image, const std::vector<ActionType>& actionSeq);

    EnumBitSet<OpFlags, OpFlags::Max> flags;
    std::vector<ActionType> actions;
    static const NameRegistry<OpInfo> opTable;
};

#endif
