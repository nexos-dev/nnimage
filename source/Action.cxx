/*
    Action.cxx - contains action handling code
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
#include "include/ActionOptions.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>

// Returns an action object given a name
std::unique_ptr<Action> Action::MakeAction (const std::string& name)
{
    auto it = actionTable.find (name);
    if (it == actionTable.end())
        return nullptr;
    return it->second();
}

bool Action::SetOption (OptionId id, const std::string& val)
{
    switch (id)
    {
        case OptionId::ConfFile:
            conf.fileName = val;
            break;
        case OptionId::NamePrefix:
            namePrefix = val;
            break;
        case OptionId::OutputDir:
            outputDir = val;
            break;
        case OptionId::Backend:
            // Resolve the backend
            backendType = Backend::ResolveBackend (val);
            if (backendType == BackendType::None)
            {
                _log->Error ("invalid backend \"" + val + "\"");
                return false;
            }
            break;
        case OptionId::ConfEnc:
            conf.fileEnc = val;
            break;
        case OptionId::FailOnSkip:
            failOnSkip = true;
            break;
    }
    return true;
}

bool Action::ValidateOptions()
{
    // If output directory is not set, use current working directory
    if (outputDir.empty())
    {
        auto cwd = std::filesystem::current_path();
        outputDir = cwd.string();
        // Ensure there is a trailing slash
        if (outputDir.back() != '/')
            outputDir += '/';
    }
    return true;
}

void Action::AddPartition (std::unique_ptr<Partition> part)
{
    parts[part->GetName()] = std::move (part);
}

Image* Action::FindImage (const std::string& name)
{
    auto it =
        std::find_if (images.begin(), images.end(), [&name] (const std::unique_ptr<Image>& img) {
            return img->GetName() == name;
        });
    if (it == images.end())
        return nullptr;
    return it.base()->get();
}

bool Action::ResolvePartitions (ConfError& e)
{
    for (int i = 0; i < images.size(); ++i)
    {
        Image* img = images[i].get();
        // Resolve it
        if (!img->ResolvePartitions (e))
            return false;
    }
    return true;
}

bool Action::getConfirmation (const std::string& msg)
{
    std::string input;
    std::cout << msg << " [y/N]: ";
    std::getline (std::cin, input);
    if (input == "y" || input == "Y")
        return true;
    return false;
}

std::shared_ptr<Backend> Action::getBackend (const Image& img, BackendType type)
{
    // Get the real backend type
    type = img.GetBackendType (type);
    // Check if it was able to resolve a backend
    if (type == BackendType::None)
    {
        _log->Error ("no valid backend found for image \"" + img.GetName() + "\"");
        return nullptr;
    }
    // Yes I'm aware that doing a static_cast from an enum class is bad practice and no I don't care
    size_t idx = static_cast<size_t> (type);
    if (idx < backends.size() && backends[idx] != nullptr)
    {
        return backends[idx];
    }
    // Create the backend
    auto backend = Backend::BackendFactory (type);
    assert (backend != nullptr);
    if (!backend->BackendCreated())
        return nullptr;
    // Ensure the backends vector is large enough
    if (idx >= backends.size())
        backends.resize (idx + 1);
    backends[idx] = std::move (backend);
    return backends[idx];
}

// NOTE: right now this function fails the whole action.
// It might be better to just skip the image and continue, but that will be a future improvement
bool Action::prepareBackends (const std::vector<std::unique_ptr<Image>>& images)
{
    for (const auto& img : images)
    {
        auto backend = getBackend (*img, backendType);
        if (!backend)
            return false;
        img->SetBackend (backend);
        // Add the image to the backend
        std::string fileName = GetImageFile (*img);
        if (!backend->AddImage (*img, fileName, false))
        {
            _log->Error ("failed to add image \"" + img->GetName() + "\" to backend \"" +
                         Backend::GetBackendName (backendType) + "\"");
            return false;
        }
    }
    return true;
}

bool Action::skipImage (const Image& img)
{
    if (failOnSkip)
        return false;
    _log->Warn ("skipping image \"" + img.GetName() +
                "\", pass -fail-on-skip to treat this as an error");
    // Remove it
    // Disabled for now
    // std::erase_if (images,
    //               [&img] (const std::unique_ptr<Image>& i) { return i.get() == img.get(); });
    return true;
}

void Action::rollBackQueuedTasks()
{
    while (!addedTasks.empty())
    {
        TaskId id = addedTasks.front();
        addedTasks.pop();
        graph->RemoveTask (id);
    }
}

bool Action::skipOrFail (const Image& img, const std::string& msg)
{
    rollBackQueuedTasks();
    if (!msg.empty())
        _log->Error (msg);
    return skipImage (img);
}

TaskId Action::addTaskToGraph (std::unique_ptr<Task> task)
{
    TaskId id = graph->AddTask (std::move (task));
    addedTasks.push (id);
    return id;
}

// Create action implementation
bool CreateAction::ValidateOptions()
{
    return Action::ValidateOptions();
}

bool CreateAction::SetOption (OptionId opt, const std::string& val)
{
    if (opt == OptionId::Overwrite)
        overwrite = true;
    else
        return Action::SetOption (opt, val);
    return true;
}

// TODO: split this into multiple functions to make it more readable
// I'm sure that will happen naturally as I implement more actions, but for now this is fine
bool CreateAction::Execute()
{
    // Prepare all needed backends
    if (!prepareBackends (images))
        return false;
    // Iterate through each image
    for (auto& img : images)
    {
        clearAddedTasks();    // Ensure we start with a clean slate
        // Validate it
        if (!img->Validate())
        {
            // Skip it if we are allowed to
            if (skipOrFail (*img, "image validation failed"))
                continue;
            return false;
        }
        // Get the file name
        std::string fileName = GetImageFile (*img);
        // Check if it exists
        if (std::filesystem::exists (fileName) && !overwrite)
        {
            // Request user confirmation
            if (!getConfirmation ("image file \"" + fileName + "\" already exists. Overwrite?"))
            {
                if (skipOrFail (*img,
                                "image file \"" + fileName +
                                    "\" already exists and user did not confirm overwrite"))
                    continue;
                return false;
            }
            else
                _log->Info ("pass \"-w\" to force overwrite of existing image files");
        }
        // Get the backend
        auto backend = img->GetBackend();
        // Now invoke the backend to create the image
        auto task = backend->CreateImage (*img, fileName);
        if (task == nullptr)
        {
            if (skipOrFail (*img, ""))
                continue;
            return false;
        }
        // Add to task graph
        auto createId = addTaskToGraph (std::move (task));
        // Do the same for the partition table
        auto partTask = backend->CreatePartTable (*img, fileName);
        if (partTask == nullptr)
        {
            if (skipOrFail (*img, ""))
                continue;
            return false;
        }
        auto partId = addTaskToGraph (std::move (partTask));
        // Ensure proper ordering
        graph->AddDependency (partId, createId);
        // That's a success for this image
        ++imageSuccessCount;
    }
    // CHeck if any images were successfully processed
    if (imageSuccessCount == 0)
    {
        _log->Error ("no images could be successfully processed, aborting");
        return false;
    }
    // Do the tasks now
    return graph->RunTasks();
}

// Partition action implementation
bool PartitionAction::ValidateOptions()
{
    return Action::ValidateOptions();
}

bool PartitionAction::SetOption (OptionId opt, const std::string& val)
{
    return Action::SetOption (opt, val);
}

bool PartitionAction::Execute()
{
    return true;
}

// Format action implementation
bool FormatAction::ValidateOptions()
{
    return Action::ValidateOptions();
}

bool FormatAction::SetOption (OptionId opt, const std::string& val)
{
    return Action::SetOption (opt, val);
}

bool FormatAction::Execute()
{
    return true;
}

// Update action implementation
bool UpdateAction::ValidateOptions()
{
    // Make sure a directory was passed
    if (srcDir.empty())
    {
        _log->Error ("source directory must be passed to action \"update\"");
        return false;
    }
    return Action::ValidateOptions();
}

bool UpdateAction::SetOption (OptionId opt, const std::string& val)
{
    if (opt == OptionId::SrcDir)
        srcDir = val;
    else
        return Action::SetOption (opt, val);
    return true;
}

bool UpdateAction::Execute()
{
    return true;
}
