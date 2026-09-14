/*
    Action.cxx - contains action handling code
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

#include "include/ActionTypes.h"

std::unique_ptr<ActionOptions> ActionOptions::MakeActOptions (ActionType type)
{
    return actionTable[type]();
}

void ActionOptions::CollectOptions (OptionsParser& opts)
{}

void CreateActOptions::CollectOptions (OptionsParser& opts)
{
    ActionOptions::CollectOptions (opts);
}

ResNone CreateActOptions::ValidateOptions()
{
    return Success();
}

void InitActOptions::CollectOptions (OptionsParser& opts)
{
    ActionOptions::CollectOptions (opts);
}

ResNone InitActOptions::ValidateOptions()
{
    return Success();
}

void PartitionActOptions::CollectOptions (OptionsParser& opts)
{}

ResNone PartitionActOptions::ValidateOptions()
{
    return Success();
}

void FormatActOptions::CollectOptions (OptionsParser& opts)
{
    ActionOptions::CollectOptions (opts);
}

ResNone FormatActOptions::ValidateOptions()
{
    return Success();
}

void UpdateActOptions::CollectOptions (OptionsParser& opts)
{
    ActionOptions::CollectOptions (opts);
}

ResNone UpdateActOptions::ValidateOptions()
{
    return Success();
}

const EnumArray<ActionType, ActOptSetter, ActionType::Max> ActionOptions::actionTable = {
    {ActionType::Create, []() { return std::make_unique<CreateActOptions>(); }},
    {ActionType::Init, []() { return std::make_unique<InitActOptions>(); }},
    {ActionType::Partition, []() { return std::make_unique<PartitionActOptions>(); }},
    {ActionType::Format, []() { return std::make_unique<FormatActOptions>(); }},
    {ActionType::Update, []() { return std::make_unique<UpdateActOptions>(); }}};
