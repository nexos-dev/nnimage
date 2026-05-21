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

bool TaskGraph::taskExists (TaskId task)
{
    if (tasks.size() < task)
        return false;
    return true;
}

TaskId TaskGraph::AddTask (TaskFunc func)
{
    TaskId id = curId;
    curId++;    // Move to next id
    auto task = std::make_unique<Task> (id, func);
    tasks.insert (tasks.begin() + id, std::move (task));
    return id;
}

bool TaskGraph::AddDependency (TaskId src, TaskId dest)
{
    // Ensure src and dest exist
    if (!taskExists (src) || !taskExists (dest))
        return false;
    // Ensure src exists in list
    if (adjList.size() <= src)
        adjList.resize (src + 1);
    // Add to adjacency list
    adjList[src].push_back (dest);
    // Now adjust in-degree
    if (inDegree.size() <= dest)
        inDegree.resize (dest + 1);
    inDegree[dest]++;
    return true;
}

int TaskGraph::getInDegree (TaskId task)
{
    if (task >= inDegree.size())
        return -1;
    return inDegree[task];
}
