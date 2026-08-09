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

#include "Option.h"
#include "BackendTypes.h"
#include "include/Image.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>

class Backend;

enum class ActionType
{
    ActionCreate,
    ActionPartition,
    ActionFormat,
    ActionUpdate
};

class Action
{
  public:
    Action() = delete;
    Action (ActionType action) : type{action}
    {}
    void AddPartition (std::unique_ptr<Partition> part);
    Image* FindImage (const std::string& name);
    bool ResolvePartitions (ConfError& e);
    const ImgConfFile& GetConf() const
    {
        return conf;
    }
    void AddImage (std::unique_ptr<Image> image)
    {
        images.push_back (std::move (image));
    }
    std::string GetImageFile (const Image& img)
    {
        return outputDir + namePrefix + img.GetName() + img.GetSpec().fileExt;
    }
    std::unique_ptr<Partition> GetPartition (const std::string& name)
    {
        auto it = parts.find (name);
        if (it == parts.end())
            return nullptr;
        return std::move (it->second);
    }
    virtual Option* GetOptions() = 0;
    virtual bool Execute() = 0;
    virtual bool SetOption (OptionId opt, const std::string& val);
    virtual bool ValidateOptions();
    static std::unique_ptr<Action> MakeAction (const std::string& name);
    // Destructor/copy stuff
    virtual ~Action() = default;
    Action (const Action&) = delete;
    Action& operator= (const Action&) = delete;

  protected:
    bool getConfirmation (const std::string& msg);
    std::shared_ptr<Backend> getBackend (const Image& img, BackendType type);
    bool prepareBackends (const std::vector<std::unique_ptr<Image>>& images);
    bool skipImage (const Image& img);
    // Rolls back all tasks that have been added to the task graph for the current working image
    void rollBackQueuedTasks();
    // Clears the list of added tasks for the current working image
    void clearAddedTasks()
    {
        addedTasks = std::queue<TaskId>();
    }
    TaskId addTaskToGraph (std::unique_ptr<Task> task);
    // Skips over the given image, and if failOnSkip is set, returns false to abort the action,
    // otherwise returns true to continue. Prints given message if failure is imminent
    bool skipOrFail (const Image& img, const std::string& msg);
    ActionType type;
    // Set when the user wants any image failure to abort the entire action, otherwise the action
    // will roll with what's left
    bool failOnSkip = false;
    size_t imageSuccessCount = 0;
    std::string outputDir;
    std::string namePrefix;
    ImgConfFile conf{"nnimage.conf", ""};
    BackendType backendType = BackendType::None;
    std::vector<std::unique_ptr<Image>> images;
    std::map<std::string, std::unique_ptr<Partition>> parts;
    std::unique_ptr<TaskGraph> graph = std::make_unique<TaskGraph>();

  private:
    // TODO: make this into an EnumArray
    std::vector<std::shared_ptr<Backend>> backends;
    // All tasks that have been added to the task graph for the current working image, used for
    // rollback
    std::queue<TaskId> addedTasks;
};

extern Action* _actionOverride;

#endif
