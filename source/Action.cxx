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
#include <cassert>

// Returns an action object given a name
std::unique_ptr<Action> Action::MakeAction (const std::string& name)
{
    auto it = actionTable.find (name);
    if (it == actionTable.end())
        return nullptr;
    return it->second();
}

bool Action::SetGlobalOption (OptionId id, const std::string& val)
{
    switch (id)
    {
        case OptionId::ConfFile:
            this->confFile = val;
            break;
        case OptionId::ImageFile:
            this->outputFile = val;
            break;
        case OptionId::Backend:
            // Resolve the backend
            this->defaultBackend = Backend::ResolveBackend (val);
            if (this->defaultBackend == BackendType::None)
            {
                _log->Error ("invalid backend \"" + val + "\"");
                return false;
            }
            break;
        case OptionId::ConfEnc:
            this->opts[id] = val;
            break;
    }
    return true;
}

bool Action::ValidateGlobalOptions()
{
    return true;
}

void Action::AddPartition (std::unique_ptr<Partition> part)
{
    this->parts[part->GetName()] = std::move (part);
}

Image* Action::FindImage (const std::string& name)
{
    auto it = std::find_if (
        this->images.begin(),
        this->images.end(),
        [&name] (const std::unique_ptr<Image>& img) { return img->GetName() == name; });
    if (it == this->images.end())
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

// Create action implementation
bool CreateAction::ValidateOptions()
{
    return true;
}

bool CreateAction::SetOption (OptionId opt, const std::string& val)
{
    if (opt == OptionId::Overwrite)
        this->overwrite = true;
    return true;
}

bool CreateAction::Execute()
{
    TaskGraph graph;
    for (auto& img : this->images)
    {
        // Validate it
        if (!img->Validate())
            return false;
    }
    return true;
}

// Partition action implementation
bool PartitionAction::ValidateOptions()
{
    return true;
}

bool PartitionAction::SetOption (OptionId opt, const std::string& val)
{
    return true;
}

bool PartitionAction::Execute()
{
    return true;
}

// Format action implementation
bool FormatAction::ValidateOptions()
{
    return true;
}

bool FormatAction::SetOption (OptionId opt, const std::string& val)
{
    return true;
}

bool FormatAction::Execute()
{
    return true;
}

// Update action implementation
bool UpdateAction::ValidateOptions()
{
    // Make sure a directory was passed
    if (this->srcDir.empty())
    {
        _log->Error ("source directory must be passed to action \"update\"");
        return false;
    }
    return true;
}

bool UpdateAction::SetOption (OptionId opt, const std::string& val)
{
    if (opt == OptionId::SrcDir)
        this->srcDir = val;
    return true;
}

bool UpdateAction::Execute()
{
    return true;
}
