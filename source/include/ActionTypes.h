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

class CreateAction : public Action
{
  public:
    CreateAction() : Action (ActionType::Create)
    {}

  private:
};

class CreateActOptions : public ActionOptions
{
  public:
    void CollectOptions (cxxopts::Options& opts) override;
    ResNone ValidateOptions() override;

    std::unique_ptr<Action> MakeAction() override
    {
        return std::make_unique<CreateAction>();
    }
};

class InitAction : public Action
{
  public:
    InitAction() : Action (ActionType::Init)
    {}

  private:
};

class InitActOptions : public ActionOptions
{
  public:
    void CollectOptions (cxxopts::Options& opts) override;
    ResNone ValidateOptions() override;

    std::unique_ptr<Action> MakeAction() override
    {
        return std::make_unique<InitAction>();
    }
};

class PartitionAction : public Action
{
  public:
    PartitionAction() : Action (ActionType::Partition)
    {}
};

class PartitionActOptions : public ActionOptions
{
  public:
    void CollectOptions (cxxopts::Options& opts) override;
    ResNone ValidateOptions() override;

    std::unique_ptr<Action> MakeAction() override
    {
        return std::make_unique<PartitionAction>();
    }
};

class FormatAction : public Action
{
  public:
    FormatAction() : Action (ActionType::Format)
    {}
};

class FormatActOptions : public ActionOptions
{
  public:
    void CollectOptions (cxxopts::Options& opts) override;
    ResNone ValidateOptions() override;

    std::unique_ptr<Action> MakeAction() override
    {
        return std::make_unique<FormatAction>();
    }
};

class UpdateAction : public Action
{
  public:
    UpdateAction() : Action (ActionType::Update)
    {}

  private:
};

class UpdateActOptions : public ActionOptions
{
  public:
    void CollectOptions (cxxopts::Options& opts) override;
    ResNone ValidateOptions() override;

    std::unique_ptr<Action> MakeAction() override
    {
        return std::make_unique<UpdateAction>();
    }
};

#endif
