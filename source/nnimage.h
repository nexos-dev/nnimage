/*
    nnimage.h - contains header of nnimage
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

#ifndef NNIMAGE_H
#define NNIMAGE_H

#include "ConfParser.h"
#include "config.h"
#include "external/MemoryMapped.h"
#include "Backend.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Option structure
enum class OptionId;
struct Option
{
    const char* shortOpt;      // Short option (e.g., "-f")
    const char* longOpt;       // Long option (e.g., "-conf")
    OptionId id;               // Id of option
    const char* helpString;    // String to put in help about this option. If null, option is
                               // undocumented
    bool requiresArg;          // Wheter this is just an option or if it takes an argument as well
    const char* argHelp;       // Help string for argument
};

#include "Option.h"

// Action base class
class Image;
enum class ActionType;
class CmdLine;
class Partition;
struct ConfError;
class Action
{
  public:
    Action() = delete;
    Action (ActionType action) : type{action}
    {}
    // Gets all valid options for action
    virtual Option* GetOptions() = 0;
    // Set specified option on action
    virtual bool SetOption (OptionId opt, const std::string& val) = 0;
    // Ensures options are in a valid state
    virtual bool ValidateOptions() = 0;
    // Executes each task for an action
    virtual bool Execute() = 0;
    // Creates an action object
    static std::unique_ptr<Action> MakeAction (const std::string& name);
    // Sets a non-action-specific option (TODO implement polymorphically)
    bool SetGlobalOption (OptionId opt, const std::string& val);
    // Validates all non-action-specific options
    bool ValidateGlobalOptions();
    // Gets configuration file name
    const std::string& GetConf()
    {
        return confFile;
    }
    // Gets value of option
    const std::string& GetOption (OptionId id)
    {
        if (opts.find (id) == opts.end())
        {
            // FIXME: there's probably a cleaner way to do this but I'm no C++ magician so I don't
            // know
            static std::string empty = "";
            return empty;
        }
        return opts[id];
    }
    // Adds image to action
    void AddImage (std::unique_ptr<Image> image)
    {
        this->images.push_back (std::move (image));
    }
    // Adds partition to action
    void AddPartition (std::unique_ptr<Partition> part);
    // Finds an image in the action
    Image* FindImage (const std::string& name);
    // Gets partition of specified name
    std::unique_ptr<Partition> GetPartition (const std::string& name)
    {
        auto it = this->parts.find (name);
        if (it == this->parts.end())
            return nullptr;
        return std::move (it->second);
    }
    // Resolves all partitions in each image
    bool ResolvePartitions (ConfError& e);
    virtual ~Action() = default;
    // Deleted copy constructors
    Action (const Action&) = delete;
    Action& operator= (const Action&) = delete;

  protected:
    ActionType type;
    // Command line values
    std::string outputFile;
    std::string confFile = "nnimage.conf";
    BackendType defaultBackend = BackendType::None;
    std::unordered_map<OptionId, std::string> opts;
    // All images and partitions
    std::vector<std::unique_ptr<Image>> images;
    std::map<std::string, std::unique_ptr<Partition>> parts;
};

#include "Actions.h"

// Help and version callbacks
typedef void (*HelpCb)();
typedef void (*VersionCb)();

// Command line handing class
class CmdLine
{
  public:
    CmdLine() = delete;
    CmdLine (int argc, char** argv);
    // Parses all command line arguments, returing bool if there was an error
    bool ParseArguments (HelpCb helpCallback, VersionCb versionCallback);
    // Gets action we need to process
    Action* GetAction()
    {
        return action.get();
    }

  private:
    std::vector<std::string> rawArgs;
    std::unique_ptr<Action> action;
    const Option* findOptionInArray (const std::string& opt, const Option* opts);
};

// Log levels
enum class LogLevel
{
    Error,
    Warning,
    Info,
    Verbose
};

// Class for error handling
class Log
{
  public:
    Log()
    {}
    Log (const char* progName, LogLevel defaultLevel);
    void Error (const std::string& msg);
    void Warn (const std::string& msg);
    void Info (const std::string& msg);
    void SysError (const std::string& msg);
    bool AddFile (const std::string& fileName);
    ~Log();
    void SetLogLevel (LogLevel level);

  private:
    // Log files
    std::vector<std::ofstream> logs;
    // Level we are logging at
    LogLevel logLevel;
    // Cout flags
    bool isCoutTty;
    // Cerr flags
    bool isCerrTty;
    // Lines that have been logged
    std::list<std::string> lines;
    // Program name
    const char* prog;
    // Start time of log (used for log file output)
    time_t logStartTime;
    // Adds a line of text to log files
    void addLine (const std::string& line, LogLevel level);
    // Makes path for log
    const std::string getLogPath (const std::string& logName);
};

enum class ConfValType
{
    Invalid,
    String,
    Integer,
    NumId
};

// Errors for proccesing
enum class ConfErrorType
{
    Ok,
    BadType,
    NoName,
    BadProp,
    ExtraVals,
    PropRequired,
    WrongType,
    UnrecognizedId,
    BadNumId,
    UnrecognizedVal,
    SysError
};

struct ConfError
{
    ConfError()
    {}
    ConfError (ConfErrorType type, const ParseProp& prop)
        : code{type}, msg{prop.name}, line{prop.line}
    {}
    ConfError (ConfErrorType type, const std::string& msg, int line)
        : code{type}, msg{msg}, line{line}
    {}
    ConfErrorType code = ConfErrorType::Ok;
    std::string msg;
    int line;
};

// Image configuration proccesor
class Image;
class Partition;
class ImageConf
{
  public:
    ImageConf() = delete;
    ImageConf (const std::string& fileName) : fileName{fileName}
    {}
    std::vector<std::unique_ptr<Image>>& GetImages()
    {
        return images;
    }
    std::unique_ptr<Partition> GetPartition (const std::string& name)
    {
        auto it = this->parts.find (name);
        if (it == this->parts.end())
            return nullptr;
        return std::move (it->second);
    }
    bool ParseFile();
    ParseProp& GetProp (ParseBlock& block,
                        const std::string& name,
                        ConfType expectedType,
                        int maxVals,
                        ConfError& result);
    void RemoveProp (ParseBlock& block, ParseProp& prop);
    bool ValidateProp (const ParseProp& prop, ConfType expectedType, int maxVal, ConfError& result);
    bool GetVal (const ParseProp& prop, ConfVal& out, int idx);
    int GetNumVals (ParseProp& prop);

  private:
    bool convertFileEnc (std::string_view data, size_t dataSize, std::string& out, const char* enc);
    bool openConfFile (MemoryMapped& file, std::string_view& contents);
    bool processImageBlock (ParseBlock& block);
    bool processPartitionBlock (ParseBlock& block);
    void parseError (ConfErrorType error, int line, const std::string& extra);
    bool addPartitions (Image* img, const ParseProp& block);
    std::vector<std::unique_ptr<Image>> images;
    std::map<std::string, std::unique_ptr<Partition>> parts;
    const std::string& fileName;
};

using ImgConfSetter = std::function<void (Image&, const ConfVal&, ConfErrorType&)>;
using PartConfSetter = std::function<void (Partition&, const ConfVal&, ConfErrorType&)>;

struct ImgConfItem
{
    ConfType type;
    ImgConfSetter setter;
};

struct PartConfItem
{
    ConfType type;
    PartConfSetter setter;
};

enum class PartProp;
class Partition
{
  public:
    Partition (const std::string& name) : name{name}
    {}
    const std::string& GetName()
    {
        return name;
    }
    // Sets a configuration key in the image
    void SetConf (const std::string& name, const ConfVal& val, ConfError& e);
    void SetConf (PartProp key, const ConfVal& val, ConfError& e);
    // Resolves name of property
    PartProp ResolveName (const std::string& name);

  private:
    // Gets name from property
    const std::string getPropName (PartProp prop);
    const std::string name;    // Name of partition
    std::string fs;            // Filesystem used on partition
    std::string prefix;        // Prefix in output directory of partition
    int64_t start;             // Start position of partition
    int64_t size;              // Size of partitions in bytes
    bool isBoot;               // Wheter this is the boot partition
    const static std::unordered_map<PartProp, PartConfItem> registry;    // Configuration
                                                                         // registry
    const static std::unordered_map<std::string, PartProp> nameRegistry;
};

// Partition reference
class PartRef
{
  public:
    PartRef (const std::string& name, int line) : name{name}, line{line}
    {}
    const std::string GetName() const
    {
        return name;
    }
    int GetLine() const
    {
        return line;
    }

  private:
    const std::string name;    // Name of partition being referenced
    int line;                  // Line of reference
};

// Defined image types
enum class ImageType
{
    Mbr,
    Gpt,
    Iso9660,
    Floppy,
    Error
};

// Defined image boot modes
enum class BootMode
{
    None,
    Bios,
    Efi,
    Error
};

// Image class
enum class ImgProp;
class Image
{
  public:
    Image (const std::string& name, ImageType type) : name{name}, type{type}
    {}
    const std::string& GetName()
    {
        return name;
    }
    ImageType GetType()
    {
        return type;
    }
    // Sets a configuration key in the image
    void SetConf (const std::string& name, const ConfVal& val, ConfError& e);
    void SetConf (ImgProp prop, const ConfVal& val, ConfError& e);
    // Adds a partition to it
    void AddPartition (PartRef& ref)
    {
        partitionNames.push_back (ref);
    }
    // Adds a partition by partition object
    void AddPartition (std::unique_ptr<Partition> part)
    {
        parts.push_back (std::move (part));
    }
    // Resolves all partitions
    bool ResolvePartitions (ConfError& e);
    // Validates the image's configuration
    virtual bool Validate();
    // Creates an image object from a type and name
    static std::unique_ptr<Image> ImageFactory (const std::string& type, const std::string& name);
    // Converts a numid into a number
    static int64_t NormalizeNumId (const ConfNumId& numId);
    // Resolves a property name into it's key
    static ImgProp ResolveProp (const std::string& name);
    // Gets name from image type
    static const std::string GetTypeName (ImageType type);
    // Virtual destructor to avoid compiler warnings
    virtual ~Image() = default;

  protected:
    // Gets numeric boot mode from string boot mode
    virtual BootMode getBootMode (const std::string& modeStr) = 0;
    // Gets registry of configuration keys
    virtual const std::unordered_map<ImgProp, ImgConfItem>& getRegistry() = 0;
    // Data fields
    ImageType type = ImageType::Gpt;
    const std::string name;
    int64_t size = -1;    // In bytes
    BootMode bootMode = BootMode::None;
    std::vector<PartRef> partitionNames;              // List of partitions before resolution
    std::vector<std::unique_ptr<Partition>> parts;    // List after resolution
  private:
    // Gets name from ImgProp
    const std::string getPropName (ImgProp prop);
    // Registry databases
    const static std::unordered_map<ImgProp, ImgConfItem> baseRegistry;
    const static std::unordered_map<std::string, ImgProp> nameRegistry;
};

#include "ImageTypes.h"

using TaskFunc = std::function<bool()>;
typedef int TaskId;

// Task class
class Task
{
  public:
    Task (TaskId id, TaskFunc func) : id{id}, task{func}
    {}
    bool Run()
    {
        return task();
    }
    TaskId GetId()
    {
        return id;
    }

  private:
    TaskId id;
    TaskFunc task;
};

class TaskGraph
{
  public:
    TaskId AddTask (TaskFunc func);
    bool AddDependency (TaskId src, TaskId dest);    // Adds dependency dest to src
    bool RunTasks();    // Runs all tasks in their proper order, as parallelized as possible
  private:
    bool taskExists (TaskId task);
    int getInDegree (TaskId task);
    std::vector<std::unique_ptr<Task>> tasks;    // Array of tasks
    std::vector<std::vector<TaskId>> adjList;    // Dependency info
    std::vector<TaskId> sortedTasks;             // Topologically sorted tasks
    std::vector<int> inDegree;                   // In degree list
    TaskId curId = 0;
};

extern std::unique_ptr<Log> _log;

static inline Action* GetAction()
{
    extern std::unique_ptr<CmdLine> _cmdLine;
    return _cmdLine->GetAction();
}

#endif
