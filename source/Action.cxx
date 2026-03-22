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
    if (name == "create")
        return std::make_unique<CreateAction> (CreateAction());
    else if (name == "partition")
        return std::make_unique<PartitionAction> (PartitionAction());
    else if (name == "format")
        return std::make_unique<FormatAction> (FormatAction());
    else if (name == "update")
        return std::make_unique<UpdateAction> (UpdateAction());
    return nullptr;
}

bool Action::SetGlobalOption (const std::string& opt, const std::string& val)
{
    if (opt == "image_file")
    {
        assert (!val.empty());
        this->outputFile = val;
    }
    else if (opt == "default_backend")
    {
        // FIXME: probably gonna make this a little cleaner
        if (val != "libkrun" && val != "libisofs")
        {
            _log->Error ("invalid backend \"" + val + "\" specified");
            return false;
        }
        this->opts[opt] = val;
    }
    else if (opt == "conf_file")
    {
        assert (!val.empty());
        this->confFile = val;
    }
    else if (opt == "conf_enc")
    {
        assert (!val.empty());
        this->opts[opt] = val;
    }
    else
        assert (!"Invalid option passed to Action::SetGlobalOption()");
    return true;
}

bool Action::ValidateGlobalOptions()
{
    // Set defaults
    if (this->confFile.empty())
        this->confFile = "nnimage.conf";
    if (GetOption ("default_backend").empty())
        this->opts["default_backend"] = "libkrun";
    // NOTE: we set default output file later on, as it can be a few different things
    return true;
}

// Create action implementation
bool CreateAction::ValidateOptions()
{
    return true;
}

bool CreateAction::SetOption (const std::string& opt, const std::string& val)
{
    if (opt == "overwrite")
        this->overwrite = true;
    else
        assert (!"Invalid option passed to CreateAction::SetOption");
    return true;
}

// Partition action implementation
bool PartitionAction::ValidateOptions()
{
    return true;
}

bool PartitionAction::SetOption (const std::string& opt, const std::string& val)
{
    return true;
}

// Format action implementation
bool FormatAction::ValidateOptions()
{
    return true;
}

bool FormatAction::SetOption (const std::string& opt, const std::string& val)
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

bool UpdateAction::SetOption (const std::string& opt, const std::string& val)
{
    if (opt == "src_directory")
        this->srcDir = val;
    else
        assert (!"Invalid source directory passed to UpdateAction::SetOption");
    return true;
}
