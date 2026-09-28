/*
    ActionTypes.h - contains actions
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

#include <memory>

class InitAction : public Action
{
  public:
    InitAction() : Action (ActionType::Init)
    {}

    ResNone FillTarget (Target& target, const Image& image) override;

  private:
};

class InitActOptions : public ActionOptionsFactory<InitAction>
{
  public:
    void CollectOptions (OptionsParser& opts) override;
    ResNone ValidateOptions() override;
};

class PartitionAction : public Action
{
  public:
    PartitionAction() : Action (ActionType::Partition)
    {}

    ResNone FillTarget (Target& target, const Image& image) override;
};

class PartitionActOptions : public ActionOptionsFactory<PartitionAction>
{
  public:
    void CollectOptions (OptionsParser& opts) override;
    ResNone ValidateOptions() override;
};

class FormatAction : public Action
{
  public:
    FormatAction() : Action (ActionType::Format)
    {}

    ResNone FillTarget (Target& target, const Image& image) override;
};

class FormatActOptions : public ActionOptionsFactory<FormatAction>
{
  public:
    void CollectOptions (OptionsParser& opts) override;
    ResNone ValidateOptions() override;
};

class UpdateAction : public Action
{
  public:
    UpdateAction() : Action (ActionType::Update)
    {}

    ResNone FillTarget (Target& target, const Image& image) override;

  private:
};

class UpdateActOptions : public ActionOptionsFactory<UpdateAction>
{
  public:
    void CollectOptions (OptionsParser& opts) override;
    ResNone ValidateOptions() override;
};

#endif
