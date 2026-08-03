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
#include "MemoryMapped.h"
#include "Backend.h"
#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
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
    // Executes each task for an action
    virtual bool Execute() = 0;
    // Creates an action object
    static std::unique_ptr<Action> MakeAction (const std::string& name);
    // Set specified option on action
    virtual bool SetOption (OptionId opt, const std::string& val);
    // Ensures options are in a valid state
    virtual bool ValidateOptions();
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
    std::string outputDir;
    std::string namePrefix;
    std::string confFile = "nnimage.conf";
    BackendType backendType = BackendType::None;
    std::unordered_map<OptionId, std::string> opts;
    // All images and partitions
    std::vector<std::unique_ptr<Image>> images;
    std::map<std::string, std::unique_ptr<Partition>> parts;
    // Requests user confirmation for something
    bool getConfirmation (const std::string& msg);
    // Gets the backend object we need for an image, given a suggestion
    std::shared_ptr<Backend> getBackend (const Image& img, BackendType type);
    // Prepares all backends for use
    bool prepareBackends (const std::vector<std::unique_ptr<Image>>& images);

  private:
    // List of all backends that have been created. This is because we may have multiple backends at
    // a time, e.g., if we have to create a regular disk image with krun and then created an ISO
    // image with Xorriso
    std::vector<std::shared_ptr<Backend>> backends;
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
    void LogAt (const std::string& msg, LogLevel level);
    void Disable();
    void Enable();
    ~Log();
    void SetLogLevel (LogLevel level);
    // Creates a stream to allow logging to the returned file descriptor to go the both stderr and
    // the log
    int CreateLogStream (const std::string& msgPrefix, LogLevel level = LogLevel::Error);

  private:
    // Log files
    std::vector<std::ofstream> logs;
    // Level we are logging at
    LogLevel logLevel;
    // Cout flags
    bool isCoutTty;
    // Cerr flags
    bool isCerrTty;
    // If log is enabled right now
    std::atomic<bool> isLogEnabled = true;
    // Log mutex
    std::mutex logMtx;
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
    // Logs to cerr
    void logToCerr (const std::string& msg, LogLevel level);
    // Logs to cout
    void logToCout (const std::string& msg, LogLevel level);
    // Opened file descriptors for streams
    std::vector<int> logStreams;
};

extern std::unique_ptr<Log> _log;

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

struct PartSpec
{
    std::string name;
    std::string fs;
    std::string prefix;
    int64_t start = -1;
    int64_t size = -1;
    bool isBoot = false;
};

enum class PartProp;
class Partition
{
  public:
    Partition (const std::string& name)
    {
        spec.name = name;
    }
    const std::string& GetName()
    {
        return spec.name;
    }
    // Sets a configuration key in the image
    void SetConf (const std::string& name, const ConfVal& val, ConfError& e);
    void SetConf (PartProp key, const ConfVal& val, ConfError& e);
    // Resolves name of property
    PartProp ResolveName (const std::string& name);
    // Gets partition specification
    const PartSpec& GetSpec()
    {
        return spec;
    }

  private:
    // Gets name from property
    const std::string getPropName (PartProp prop);
    PartSpec spec;
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

// Image specification
struct ImgSpec
{
    std::string name;
    std::string fileExt = ".img";
    ImageType type = ImageType::Gpt;
    int64_t size = -1;
    BootMode bootMode = BootMode::None;
    mutable std::mutex lock;
    // Deleted copy constructors
    ImgSpec (const ImgSpec&) = delete;
    ImgSpec& operator= (const ImgSpec&) = delete;
    ImgSpec (const ImgSpec&&) = delete;
    ImgSpec& operator= (const ImgSpec&&) = delete;
    ImgSpec() = default;
    ~ImgSpec() = default;
};

// Image class
enum class ImgProp;
class Image
{
  public:
    Image (const std::string& name, ImageType type)
    {
        spec.name = name;
        spec.type = type;
    }
    const std::string& GetName()
    {
        return spec.name;
    }
    ImageType GetType() const
    {
        return spec.type;
    }
    bool SetBackend (std::shared_ptr<Backend> backend)
    {
        if (this->backend != nullptr)
            return false;
        this->backend = backend;
        return true;
    }
    Backend* GetBackend()
    {
        return backend.get();
    }
    BackendType GetBackendType (BackendType suggestion) const;
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
    // Gets the image specification
    const ImgSpec& GetSpec() const
    {
        return spec;
    }
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
    virtual bool checkBackend (BackendType type) const = 0;
    // Image data spec
    ImgSpec spec;
    std::vector<PartRef> partitionNames;              // List of partitions before resolution
    std::vector<std::unique_ptr<Partition>> parts;    // List after resolution
    BackendType defaultBackend = BACKEND_DEFAULT;     // Default backend for image
    std::shared_ptr<Backend> backend;                 // Backend for image
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

// Task class
class Task
{
  public:
    Task (TaskFunc func, const std::string& name) : task{func}, name{name}
    {}
    Task (TaskFunc func, const std::string& name, TaskFunc rollbackFunc)
        : task{func}, name{name}, rollback{rollbackFunc}
    {}
    void SetId (TaskId id)
    {
        this->id = id;
    }
    bool Run()
    {
        // Ensure we are in a pending state
        TaskState expected = TaskState::Pending;
        bool result = false;
        if (!state.compare_exchange_strong (expected, TaskState::Running))
        {
            if (expected == TaskState::Skipped)
                return true;
            else
            {
                _log->Error ("task \"" + name + "\" is not in a pending state");
                return false;
            }
        }
        // Do it
        result = task();
        if (result)
        {
            // If a rollback occured, do it now
            if (rollbackPending.load())
            {
                rollback();
                this->state.store (TaskState::RolledBack);
            }
            else
                this->state.store (TaskState::Finished);
        }
        else
            this->state.store (TaskState::Failed);
        return result;
    }
    TaskState Skip()
    {
        // Ensure we are pending
        TaskState expected = TaskState::Pending;
        if (!state.compare_exchange_strong (expected, TaskState::Skipped))
            return expected;
        return TaskState::Skipped;
    }
    bool Rollback()
    {
        // Ensure we are finished
        TaskState expected = TaskState::Finished;
        if (!state.compare_exchange_strong (expected, TaskState::Rollback))
            return false;
        rollback();
        this->state.store (TaskState::RolledBack);
        return true;
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
    static std::unique_ptr<Task> EmptyTask (const std::string& name = "NoopTask")
    {
        return std::make_unique<Task> ([]() { return true; }, name);
    }
    void SetRollbackPending()
    {
        rollbackPending.store (true);
    }

  private:
    TaskId id;
    TaskFunc task;
    TaskFunc rollback = []() {
        _log->Warn ("default rollback handler called");
        return true;
    };
    std::atomic<TaskState> state{TaskState::Pending};
    std::atomic<bool> rollbackPending{false};
    const std::string name;
};

class TaskGraph
{
  public:
    TaskGraph()
    {}
    TaskId AddTask (std::unique_ptr<Task> task);
    bool AddDependency (TaskId src, TaskId dest);    // Adds dependency dest to src
    bool RunTasks();    // Runs all tasks in their proper order, as parallelized as possible
  private:
    bool taskExists (TaskId task);
    int getInDegree (TaskId task);
    bool pathExists (TaskId src, TaskId dest);
    bool rollbackTasks (TaskId failedTask);
    bool topoSort (std::queue<TaskId>& sortedTasks);
    void getDescendants (TaskId task, std::vector<TaskId>& descendants);
    void addReadyTask (TaskId task);
    std::vector<std::unique_ptr<Task>> tasks;    // Array of tasks
    std::queue<TaskId> completedQueue;    // Queue of completed tasks, reverse topological order
    std::mutex completedMtx;              // Mutex for completed queue
    std::vector<std::vector<TaskId>> adjList;    // Dependency info
    std::vector<TaskId> sortedTasks;             // Topologically sorted tasks
    std::deque<std::atomic<int>> inDegree;       // In degree list
    std::mutex readyMtx;                         // Mutex for ready queue
    std::queue<TaskId> ready;                    // Ready queue
    std::condition_variable readyCond;           // Condition variable for ready queue
    std::mutex failureMtx;                       // Mutex for failure state
    TaskId curId = 0;
    std::atomic<size_t> completed = 0;
    std::atomic<size_t> succeeded = 0;
};

static inline Action* GetAction()
{
    extern std::unique_ptr<CmdLine> _cmdLine;
    return _cmdLine->GetAction();
}

// Test driver function
bool TestDriver (int argc, char** argv);

#endif
