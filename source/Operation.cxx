/*
    Operation.cxx - contains operation handling code
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

#include "include/Operation.h"
#include "include/Frontend.h"
#include "include/OpTypes.h"

Result<std::unique_ptr<Operation>> Operation::MakeOperation (std::string_view opName, OperationOptions& opOptions)
{
    const OpInfo* info = opTable.Find (opName);
    if (!info)
        return Error ({ErrorDomain::Operation, ErrorCode::UnknownOperation}, {{"name", std::string (opName)}});

    std::unique_ptr<Operation> op;

    if (info->type == OpType::OpMeta)
        op = info->factory (opOptions);
    else
        op = std::make_unique<Operation> (Operation (opOptions, info->actions));

    if (op == nullptr)
        return op;
    op->flags = info->flags;
    return op;
}

Result<Target> Operation::buildTarget (const Image& image, const std::vector<ActionType>& actionSeq)
{
    if (actionSeq.empty())
        return Error ({ErrorDomain::Operation, ErrorCode::NoActionsSelected}, {{"name_suffix", ""}});

    Target target (image.GetName());
    size_t idx = 0;
    for (ActionType type : actionSeq)
    {
        std::unique_ptr<Action> action = opOptions.MakeAction (type);
        auto result = action->FillTarget (target, image);
        if (!result)
            return result.Error();
    }
    return target;
}

Result<std::vector<Target>> Operation::PrepareTargets (const ImageSet& images)
{
    std::vector<Target> targets;
    targets.reserve (images.size());

    for (const auto& image : images)
    {
        const Image& img = *image.second;
        auto resActions = SelectActions (img);
        if (!resActions)
            return resActions.Error();

        auto resTarget = buildTarget (img, resActions.Value());
        if (!resTarget)
            return resTarget.Error();

        targets.push_back (std::move (resTarget.Value()));
    }
    return targets;
}

OperationOptions::OperationOptions()
{
    // Initialize every action
    for (auto it = actOptions.begin(); it != actOptions.end(); it++)
        *it = ActionOptions::MakeActOptions (actOptions.index (it));
}

void OperationOptions::CollectOptions (OptionsParser& opts)
{
    for (auto& curOpts : actOptions)
        curOpts->CollectOptions (opts);
}

ResNone OperationOptions::ValidateOptions()
{
    for (auto& curOpts : actOptions)
    {
        auto res = curOpts->ValidateOptions();
        if (!res)
            return res;
    }
    return Success();
}

Result<std::vector<ActionType>> CreateOperation::SelectActions (const Image& image)
{
    // TODO: actually determine what needs to be done for the action based on the image
    return std::vector<ActionType>{ActionType::Init, ActionType::Partition, ActionType::Format};
}
