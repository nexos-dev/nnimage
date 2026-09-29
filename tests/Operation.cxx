/*
    Operation.cxx - contains operation test cases
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
#include "include/OpTable.h"

#include <memory>
#include <vector>

TEST_CASE ("Operation::MakeOperation reports an error for an unknown operation name")
{
    OperationOptions opOptions;
    auto res = Operation::MakeOperation ("not-a-real-operation", opOptions);
    CHECK_FALSE (res);
}

TEST_CASE ("Operation::MakeOperation builds a generic operation for a plain action entry")
{
    OperationOptions opOptions;
    auto res = Operation::MakeOperation ("init", opOptions);
    REQUIRE (res);
    CHECK (res.Value() != nullptr);
}

TEST_CASE ("PrepareTargets produces one target per image for a plain action operation")
{
    OperationOptions opOptions;
    auto res = Operation::MakeOperation ("init", opOptions);
    REQUIRE (res);

    std::unique_ptr<Image> first = std::make_unique<Image> ("first");
    std::unique_ptr<Image> second = std::make_unique<Image> ("second");
    ImageSet images;
    REQUIRE (images.AddImage (std::move (first)));
    REQUIRE (images.AddImage (std::move (second)));

    auto resTargets = res.Value()->PrepareTargets (images);
    REQUIRE (resTargets);
    CHECK (resTargets.Value().size() == 2);
}

TEST_CASE ("PrepareTargets uses the meta-operation's custom action selection")
{
    OperationOptions opOptions;
    auto res = Operation::MakeOperation ("create", opOptions);
    REQUIRE (res);

    std::unique_ptr<Image> disk = std::make_unique<Image> ("disk");
    ImageSet images;
    REQUIRE (images.AddImage (std::move (disk)));

    auto resTargets = res.Value()->PrepareTargets (images);
    REQUIRE (resTargets);
    CHECK (resTargets.Value().size() == 1);
}
