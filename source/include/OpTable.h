/*
    OpTable.h - contains operation table
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

#ifndef OPTABLE_H
#define OPTABLE_H

#include "include/NameRegistry.h"
#include "include/Operation.h"
#include "include/OpTypes.h"

// clang-format off
inline const NameRegistry<OpInfo> Operation::opTable = {
    {"init", {OpType::OpAction, OpFlags::CreatesImage, {ActionType::Init}}},
    {"create",
        {OpType::OpMeta, OpFlags::CreatesImage, {}, 
            [] (OperationOptions& opOptions) -> std::unique_ptr<Operation> {
                    return std::make_unique<CreateOperation> (opOptions);
            }
        }
    },
    {"partition",
        {OpType::OpAction, {}, {ActionType::Partition}}
    },
    {"format",
        {OpType::OpAction, {}, {ActionType::Format}}
    },
    {"update",
        {OpType::OpAction, {}, {ActionType::Update}}
    }
};

#endif
