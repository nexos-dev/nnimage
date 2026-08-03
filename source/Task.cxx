/*
    Task.cxx - contains task system implementation
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

#include "nnimage.h"

#include <condition_variable>
#include <unordered_set>
#include <mutex>
#include <queue>
#include <thread>

bool TaskGraph::pathExists (TaskId src, TaskId dest)
{
    if (src == dest)
        return true;

    std::vector<bool> visited (adjList.size(), false);
    std::queue<TaskId> pending;
    pending.push (src);
    visited[src] = true;

    while (!pending.empty())
    {
        TaskId cur = pending.front();
        pending.pop();

        for (TaskId next : adjList[cur])
        {
            if (next == dest)
                return true;
            if (visited[next])
                continue;
            visited[next] = true;
            pending.push (next);
        }
    }

    return false;
}

bool TaskGraph::taskExists (TaskId task)
{
    if (task < 0)
        return false;
    if (task >= tasks.size())
        return false;
    return true;
}

TaskId TaskGraph::AddTask (std::unique_ptr<Task> task)
{
    TaskId id = curId;
    task->SetId (id);
    tasks.push_back (std::move (task));
    adjList.emplace_back();
    inDegree.emplace_back (0);
    curId++;
    return id;
}

bool TaskGraph::AddDependency (TaskId src, TaskId dest)
{
    if (!taskExists (src) || !taskExists (dest))
        return false;
    // A dependency edge is dest -> src (src depends on dest).
    auto& deps = adjList[dest];
    if (std::find (deps.begin(), deps.end(), src) != deps.end())
        return true;
    // Reject edges that would create cycles so the graph remains a DAG.
    if (pathExists (src, dest))
        return false;
    deps.push_back (src);
    inDegree[src]++;
    return true;
}

int TaskGraph::getInDegree (TaskId task)
{
    if (!taskExists (task))
        return -1;
    return inDegree[task].load();
}

void TaskGraph::getDescendants (TaskId task, std::vector<TaskId>& descendants)
{
    // This is a standard BFS traversal of the graph
    std::vector<bool> visited (tasks.size(), false);
    std::queue<TaskId> pending;
    pending.push (task);
    visited[task] = true;
    while (!pending.empty())
    {
        TaskId cur = pending.front();
        pending.pop();
        descendants.push_back (cur);
        for (TaskId dep : adjList[cur])
        {
            if (!visited[dep])
            {
                visited[dep] = true;
                pending.push (dep);
            }
        }
    }
}

bool TaskGraph::rollbackTasks (TaskId failedTask)
{
    std::lock_guard<std::mutex> guard (failureMtx);
    // Get all descendants of the failed task and mark them as skipped
    std::vector<TaskId> descendants;
    getDescendants (failedTask, descendants);
    size_t skippedCount = 0;
    for (TaskId id : descendants)
    {
        // This will mark the task as skipped if it is still pending
        TaskState cur = tasks[id]->Skip();
        // If the task is running when we try to skip, mark it as rollback pending
        // And then Run() will rollback when it finishes
        if (cur == TaskState::Running)
            tasks[id]->SetRollbackPending();
        else if (cur == TaskState::Skipped)
            skippedCount++;
    }
    if (skippedCount > 0)
    {
        size_t done = completed.fetch_add (skippedCount) + skippedCount;
        if (done == tasks.size())
            readyCond.notify_all();
    }
    // Now, we need to rollback all finished tasks in reverse topological order
    std::lock_guard<std::mutex> completedGuard (completedMtx);
    while (!completedQueue.empty())
    {
        TaskId id = completedQueue.front();
        completedQueue.pop();
        if (std::find (descendants.begin(), descendants.end(), id) != descendants.end())
        {
            // Rollback only will be called on tasks that are finished, so we don't need to check
            // the state here
            tasks[id]->Rollback();
        }
    }
    return true;
}

bool TaskGraph::topoSort (std::queue<TaskId>& topoQueue)
{
    sortedTasks.clear();
    std::vector<int> topoInDegree (tasks.size(), 0);
    // Initialize the queue with all tasks that have no dependencies
    for (TaskId i = 0; i < tasks.size(); i++)
    {
        topoInDegree[i] = inDegree[i].load();
        if (topoInDegree[i] == 0)
            topoQueue.push (i);
    }
    // Standard Kahn's algorithm here
    while (!topoQueue.empty())
    {
        TaskId task = topoQueue.front();
        topoQueue.pop();
        sortedTasks.push_back (task);

        for (TaskId dep : adjList[task])
        {
            topoInDegree[dep]--;
            if (topoInDegree[dep] == 0)
                topoQueue.push (dep);
        }
    }
    // Cycle check
    return sortedTasks.size() == tasks.size();
}

void TaskGraph::addReadyTask (TaskId task)
{
    std::unique_lock<std::mutex> guard (readyMtx);
    ready.push (task);
    // Let a thread know that a task is ready to run
    readyCond.notify_one();
}

bool TaskGraph::RunTasks()
{
    // CHeck if we have nothing to do
    if (tasks.empty())
    {
        _log->Info ("nothing to do");
        return true;
    }
    // Build a stable topological order first to ensure this is a DAG.
    std::queue<TaskId> topoQueue;
    if (!topoSort (topoQueue))
    {
        _log->Error ("cycle detected in task graph");
        return false;
    }
    // Initialize ready queue with all source tasks
    for (TaskId i = 0; static_cast<size_t> (i) < tasks.size(); i++)
    {
        if (inDegree[i].load() == 0)
            ready.push (i);
    }
    // Prepare worker threads
    size_t workerCount = std::thread::hardware_concurrency();
    if (workerCount == 0)
        workerCount = 1;
    workerCount = std::min (workerCount, tasks.size());

    auto worker = [&]() {
        while (true)    // Execute as long as there are tasks to run
        {
            TaskId nextTask = -1;
            {
                // Here, first we wait for a task to be ready, and then we pop it
                // We also check for failure or completion to exit the loop
                std::unique_lock<std::mutex> guard (readyMtx);
                readyCond.wait (guard,
                                [&]() { return !ready.empty() || completed == tasks.size(); });

                if (completed == tasks.size())
                    return;

                nextTask = ready.front();
                ready.pop();
            }
            // Run it outside the lock
            bool ok = tasks[nextTask]->Run();
            size_t done = completed.fetch_add (1) + 1;
            // Re-compute the in-degrees of dependent tasks
            // We do this even if the task failed, because we want to mark all dependent
            // tasks as skipped
            for (TaskId dep : adjList[nextTask])
            {
                if (inDegree[dep].fetch_sub (1) == 1)
                    addReadyTask (dep);
            }
            if (ok)
            {
                succeeded++;
                // Add to list of completed tasks in reverse topological order
                std::unique_lock<std::mutex> guard (completedMtx);
                completedQueue.push (nextTask);
            }
            else
            {
                // If an error occurred, we need to begin the rollback process.
                // Essentially, we go through every descendant, mark it as skipped,
                // and if one is running or finished,we wait for it to finish
                // and then roll it back
                rollbackTasks (nextTask);
            }
            if (done == tasks.size())
                readyCond.notify_all();
        }
    };
    // Create all worker threads
    std::vector<std::thread> workers;
    workers.reserve (workerCount);
    for (size_t i = 0; i < workerCount; i++)
        workers.emplace_back (worker);
    // Go ahead and get the ball rolling
    readyCond.notify_all();
    for (std::thread& thread : workers)
        thread.join();
    // If one task succeeded at least, warn the user and still return false.
    // If zero did, that's an error
    if (succeeded == tasks.size())
        return true;
    else if (succeeded > 0)
    {
        _log->Warn ("not all tasks completed successfully");
        return false;
    }
    else
    {
        _log->Error ("action aborted");
        return false;
    }
}
