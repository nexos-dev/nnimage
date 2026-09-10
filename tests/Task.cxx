/*
    Task.cxx - contains task test cases
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
#include "include/Task.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Event trace for task testing to keep the multithreaded tests simple
struct EventTrace
{
    void Record (const std::string& event)
    {
        std::lock_guard<std::mutex> guard (mutex);
        events.push_back (event);
    }

    std::vector<std::string> Snapshot() const
    {
        std::lock_guard<std::mutex> guard (mutex);
        return events;
    }

    size_t Count (const std::string& event) const
    {
        std::lock_guard<std::mutex> guard (mutex);
        return std::count (events.begin(), events.end(), event);
    }

    mutable std::mutex mutex;
    std::vector<std::string> events;
};

// A simple task that records an event and optionally sleeps for a specified duration
std::unique_ptr<Task> MakeTask (const std::string& name,
    EventTrace* trace,
    bool result = true,
    std::atomic<int>* counter = nullptr,
    std::chrono::milliseconds delay = std::chrono::milliseconds{0})
{
    return std::make_unique<Task> (
        [trace, name, result, counter, delay]() {
            if (delay.count() > 0)
                std::this_thread::sleep_for (delay);
            if (trace)
                trace->Record (name);
            if (counter)
                counter->fetch_add (1);
            return result;
        },
        name);
}

int FindEventIndex (const std::vector<std::string>& events, const std::string& event)
{
    auto it = std::find (events.begin(), events.end(), event);
    if (it == events.end())
        return -1;
    return static_cast<int> (std::distance (events.begin(), it));
}

/********************
 *
 * Task test cases
 *
 *********************/

TEST_CASE ("Task lifecycle edge cases")
{
    std::atomic<int> runCount{0};
    Task successTask (
        [&]() {
            runCount.fetch_add (1);
            return true;
        },
        "success");
    CHECK (successTask.Run());
    CHECK (runCount.load() == 1);
    CHECK (successTask.GetState() == TaskState::Finished);

    std::atomic<int> failCount{0};
    Task failureTask (
        [&]() {
            failCount.fetch_add (1);
            return false;
        },
        "failure");
    CHECK_FALSE (failureTask.Run());
    CHECK (failCount.load() == 1);
    CHECK (failureTask.GetState() == TaskState::Failed);

    std::atomic<int> skipCount{0};
    Task skippedTask (
        [&]() {
            skipCount.fetch_add (1);
            return true;
        },
        "skipped");
    CHECK (skippedTask.Skip() == TaskState::Skipped);
    CHECK (skippedTask.GetState() == TaskState::Skipped);
    CHECK (skipCount.load() == 0);

    std::atomic<int> rollbackCount{0};
    Task rollbackTask (
        [&]() {
            rollbackCount.fetch_add (1);
            return true;
        },
        "rollback");
    CHECK (rollbackTask.Run());
    CHECK (rollbackCount.load() == 1);
    CHECK (rollbackTask.Rollback());
    CHECK (rollbackTask.GetState() == TaskState::RolledBack);
}

TEST_CASE ("TaskGraph rejects invalid DAG edges")
{
    TaskGraph graph;
    auto a = graph.AddTask (MakeTask ("a", nullptr));
    auto b = graph.AddTask (MakeTask ("b", nullptr));
    auto c = graph.AddTask (MakeTask ("c", nullptr));

    CHECK_FALSE (graph.AddDependency (a, a));
    CHECK_FALSE (graph.AddDependency (-1, a));
    CHECK_FALSE (graph.AddDependency (a, -1));
    CHECK_FALSE (graph.AddDependency (42, a));
    CHECK_FALSE (graph.AddDependency (a, 42));

    CHECK (graph.AddDependency (b, a));
    CHECK (graph.AddDependency (b, a));
    CHECK (graph.AddDependency (c, b));
    CHECK_FALSE (graph.AddDependency (a, b));
}

TEST_CASE ("TaskGraph runs a valid DAG in dependency order")
{
    EventTrace trace;
    TaskGraph graph;

    auto a = graph.AddTask (MakeTask ("a", &trace));
    auto b = graph.AddTask (MakeTask ("b", &trace, true, nullptr, std::chrono::milliseconds{1}));
    auto c = graph.AddTask (MakeTask ("c", &trace));
    auto d = graph.AddTask (MakeTask ("d", &trace));

    CHECK (graph.AddDependency (c, a));
    CHECK (graph.AddDependency (c, b));
    CHECK (graph.AddDependency (d, c));

    CHECK (graph.RunTasks());

    const auto events = trace.Snapshot();
    CHECK (events.size() == 4);
    CHECK (trace.Count ("a") == 1);
    CHECK (trace.Count ("b") == 1);
    CHECK (trace.Count ("c") == 1);
    CHECK (trace.Count ("d") == 1);

    const int aIndex = FindEventIndex (events, "a");
    const int bIndex = FindEventIndex (events, "b");
    const int cIndex = FindEventIndex (events, "c");
    const int dIndex = FindEventIndex (events, "d");

    CHECK (aIndex >= 0);
    CHECK (bIndex >= 0);
    CHECK (cIndex >= 0);
    CHECK (dIndex >= 0);
    CHECK (aIndex < cIndex);
    CHECK (bIndex < cIndex);
    CHECK (cIndex < dIndex);
}

TEST_CASE ("TaskGraph reports a single failure")
{
    TaskGraph graph;
    auto failTask = std::make_unique<Task> ([]() { return false; }, "fail");
    Task* failPtr = failTask.get();
    graph.AddTask (std::move (failTask));

    CHECK_FALSE (graph.RunTasks());
    CHECK (failPtr->GetState() == TaskState::Failed);
}

TEST_CASE ("TaskGraph skips dependents after failure")
{
    std::atomic<int> rootRuns{0};
    std::atomic<int> dependentRuns{0};
    std::atomic<int> leafRuns{0};

    EventTrace trace;
    TaskGraph graph;

    auto rootTask = std::make_unique<Task> (
        [&]() {
            trace.Record ("root");
            rootRuns.fetch_add (1);
            return false;
        },
        "root");
    Task* rootPtr = rootTask.get();
    auto root = graph.AddTask (std::move (rootTask));

    auto dependentTask = std::make_unique<Task> (
        [&]() {
            trace.Record ("dependent");
            dependentRuns.fetch_add (1);
            return true;
        },
        "dependent");
    Task* dependentPtr = dependentTask.get();
    auto dependent = graph.AddTask (std::move (dependentTask));

    auto leafTask = std::make_unique<Task> (
        [&]() {
            trace.Record ("leaf");
            leafRuns.fetch_add (1);
            return true;
        },
        "leaf");
    Task* leafPtr = leafTask.get();
    auto leaf = graph.AddTask (std::move (leafTask));

    CHECK (graph.AddDependency (dependent, root));
    CHECK (graph.AddDependency (leaf, dependent));

    CHECK_FALSE (graph.RunTasks());

    CHECK (rootRuns.load() == 1);
    CHECK (dependentRuns.load() == 0);
    CHECK (leafRuns.load() == 0);

    CHECK (trace.Count ("root") == 1);
    CHECK (trace.Count ("dependent") == 0);
    CHECK (trace.Count ("leaf") == 0);

    CHECK (rootPtr->GetState() == TaskState::Failed);
    CHECK (dependentPtr->GetState() == TaskState::Skipped);
    CHECK (leafPtr->GetState() == TaskState::Skipped);

    (void) root;
    (void) dependent;
    (void) leaf;
}

TEST_CASE ("TaskGraph reports an empty graph as success")
{
    TaskGraph graph;
    CHECK (graph.RunTasks());
}

TEST_CASE ("TaskGraph handles a larger independent workload")
{
    constexpr int taskCount = 48;
    std::atomic<int> runCount{0};
    EventTrace trace;
    TaskGraph graph;
    std::vector<Task*> tasks;
    std::random_device rd;
    std::mt19937 gen (rd());

    using ChronoMs = std::chrono::milliseconds;
    ChronoMs minDelay{1};
    ChronoMs maxDelay{100};
    std::uniform_int_distribution<ChronoMs::rep> timeDistrib (minDelay.count(), maxDelay.count());
    tasks.reserve (taskCount);

    for (int i = 0; i < taskCount; ++i)
    {
        auto task = MakeTask ("task-" + std::to_string (i),
            &trace,
            true,
            &runCount,
            std::chrono::milliseconds{timeDistrib (gen)});
        tasks.push_back (task.get());
        graph.AddTask (std::move (task));
    }

    CHECK (graph.RunTasks());
    CHECK (runCount.load() == taskCount);
    CHECK (trace.Snapshot().size() == taskCount);

    for (Task* task : tasks)
        CHECK (task->GetState() == TaskState::Finished);
}

TEST_CASE ("Task rollback pending triggers rollback handler")
{
    std::atomic<int> taskCount{0};
    std::atomic<int> rollbackCount{0};
    Task rollbackTask (
        [&]() {
            taskCount.fetch_add (1);
            return true;
        },
        "rollback-pending",
        "",
        [&]() {
            rollbackCount.fetch_add (1);
            return true;
        });

    rollbackTask.SetRollbackPending();

    CHECK (rollbackTask.Run());
    CHECK (taskCount.load() == 1);
    CHECK (rollbackCount.load() == 1);
    CHECK (rollbackTask.GetState() == TaskState::RolledBack);
}
