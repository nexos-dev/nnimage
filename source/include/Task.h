/*
    Task.h - task graph declarations
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

#ifndef NNIMAGE_TASK_H
#define NNIMAGE_TASK_H

#include "include/Log.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

using TaskFunc = std::function<bool()>;
typedef int TaskId;

enum class TaskState
{
    Pending,
    Running,
    Finished,
    Skipped,
    Rollback,
    RolledBack,
    Failed
};

class Task
{
  public:
    Task (TaskFunc func, std::string name, std::string msg = "")
        : task{func}, name{std::move (name)}, msg{std::move (msg)}
    {}
    void SetId (TaskId id)
    {
        this->id = id;
    }
    bool Run()
    {
        // Ensure the task is in a pending state and honor skipped tasks
        TaskState expected = TaskState::Pending;
        bool result = false;

        if (!state.compare_exchange_strong (expected, TaskState::Running))
        {
            if (expected == TaskState::Skipped)
                return true;
            Log::The().Error ("task \"" + name + "\" is not in a pending state");
            return false;
        }

        if (!msg.empty())
            Log::The().Status (msg);

        result = task();

        if (result)
            this->state.store (TaskState::Finished);
        else
            this->state.store (TaskState::Failed);
        return result;
    }
    TaskState Skip()
    {
        // Only a pending task can be skipped, otherwise return the current state
        TaskState expected = TaskState::Pending;
        if (!state.compare_exchange_strong (expected, TaskState::Skipped))
            return expected;
        return TaskState::Skipped;
    }
    TaskId GetId() const
    {
        return id;
    }
    const std::string& GetName() const
    {
        return name;
    }
    TaskState GetState() const
    {
        return state.load();
    }
    static std::unique_ptr<Task> EmptyTask (std::string name = "NoopTask")
    {
        return std::make_unique<Task> ([]() { return true; }, std::move (name));
    }

  private:
    TaskId id;
    TaskFunc task;
    std::atomic<TaskState> state{TaskState::Pending};
    const std::string name;
    const std::string msg = "";
};

class TaskGraph
{
  public:
    TaskGraph()
    {}
    TaskId AddTask (std::unique_ptr<Task> task);
    bool AddDependency (TaskId src, TaskId dest);
    bool RunTasks();

  private:
    bool taskExists (TaskId task);
    bool pathExists (TaskId src, TaskId dest);
    void skipDescendants (TaskId failedTask);
    bool topoSort (std::queue<TaskId>& sortedTasks);
    void getDescendants (TaskId task, std::vector<TaskId>& descendants);
    void addReadyTask (TaskId task);

    std::vector<std::unique_ptr<Task>> tasks;
    std::queue<TaskId> completedQueue;
    std::mutex completedMtx;
    std::vector<std::vector<TaskId>> adjList;
    std::vector<TaskId> sortedTasks;
    std::deque<std::atomic<int>> inDegree;
    std::mutex readyMtx;
    std::queue<TaskId> ready;
    std::condition_variable readyCond;
    std::mutex failureMtx;
    TaskId curId = 0;
    std::atomic<size_t> completed = 0;
    std::atomic<size_t> succeeded = 0;
};

#endif
