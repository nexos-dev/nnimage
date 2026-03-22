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
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Option structure
struct Option
{
    const char* shortOpt;      // Short option (e.g., "-f")
    const char* longOpt;       // Long option (e.g., "-conf")
    const char* optName;       // Consistent option name to be given to action handler
    const char* helpString;    // String to put in help about this option. If null, option is
                               // undocumented
    bool requiresArg;          // Wheter this is just an option or if it takes an argument as well
    const char* argHelp;       // Help string for argument
};

#include "Option.h"

class CmdLine;

enum class ActionType;

// Action base class
class Action
{
  public:
    Action() = delete;
    Action (ActionType action) : type{action}
    {
    }
    virtual Option* GetOptions() = 0;
    virtual bool SetOption (const std::string& opt, const std::string& val) = 0;
    virtual bool ValidateOptions() = 0;
    static std::unique_ptr<Action> MakeAction (const std::string& name);
    bool SetGlobalOption (const std::string& opt, const std::string& val);
    bool ValidateGlobalOptions();
    const std::string& GetConf()
    {
        return confFile;
    }
    const std::string& GetOption (const std::string& option)
    {
        if (opts.find (option) == opts.end())
        {
            // FIXME: there's probably a cleaner way to do this but I'm no C++ magician so I don't
            // know
            static std::string empty = "";
            return empty;
        }
        return opts[option];
    }
    virtual ~Action() = default;

  protected:
    ActionType type;
    // Command line values
    std::string outputFile;
    std::string confFile;
    std::unordered_map<std::string, std::string> opts;
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
    {
    }
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
    SysError
};

struct ConfError
{
    ConfError()
    {
    }
    ConfError (ConfErrorType type, const ParseProp& prop)
        : code{type}, msg{prop.name}, line{prop.line}
    {
    }
    ConfErrorType code = ConfErrorType::Ok;
    std::string msg;
    int line;
};

// Image configuration proccesor
class Image;
class ImageConf
{
  public:
    ImageConf() = delete;
    ImageConf (const std::string& fileName) : fileName{fileName}
    {
    }
    std::vector<std::unique_ptr<Image>>& GetImages()
    {
        return images;
    }
    bool ParseFile();
    ParseProp& GetProp (ParseBlock& block,
                        const std::string& name,
                        PropType expectedType,
                        int maxVals,
                        ConfError& result);
    void RemoveProp (ParseBlock& block, ParseProp& prop);
    bool ValidateProp (const ParseProp& prop, PropType expectedType, int maxVal, ConfError& result);
    bool GetVal (const ParseProp& prop, ParseVal& out, int idx);
    int GetNumVals (ParseProp& prop);

  private:
    bool convertFileEnc (std::string_view data, size_t dataSize, std::string& out, const char* enc);
    bool openConfFile (MemoryMapped& file, std::string_view& contents);
    bool processImageBlock (ParseBlock& block);
    bool processPartitionBlock (ParseBlock& block);
    void parseError (ConfErrorType error, int line, const std::string& extra);
    bool addPartitions (const ParseProp& block);
    std::vector<std::unique_ptr<Image>> images;
    const std::string& fileName;
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
enum class ImageType;
struct ConfError;
using Setter = std::function<void (Image&, ImageConf&, const ParseVal&, ConfErrorType&)>;
class Image
{
  public:
    Image (const std::string& name, ImageType type) : name{name}, type{type}
    {
    }
    // Sets a configuration key in the image
    virtual void SetConf (const ParseProp& prop,
                          const std::string& name,
                          ImageConf& conf,
                          ConfError& e);
    // Creates an image object from a type and name
    static std::unique_ptr<Image> ImageFactory (const std::string& type, const std::string& name);
    // Converts a numid into a number
    static int64_t NormalizeNumId (const ConfNumId& numId);
    virtual ~Image() {};

  protected:
    // Gets numeric boot mode from string boot mode
    virtual BootMode getBootMode (const std::string& modeStr) = 0;
    // Configuration registry
    const static std::unordered_map<std::string, Setter> baseRegistry;
    // Data fields
    ImageType type;
    const std::string name;
    int64_t size;    // In bytes
};

#include "ImageTypes.h"

extern Log* _log;
extern CmdLine* _cmdLine;

static inline Action* GetAction()
{
    return _cmdLine->GetAction();
}

#endif
