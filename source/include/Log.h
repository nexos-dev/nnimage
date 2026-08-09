/*
    Log.h - logging declarations
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

#ifndef LOG_H
#define LOG_H

#include "include/Error.h"
#include "include/EnumArray.h"
#include "include/SimpleLexer.h"
#include "include/LockFile.h"
#include "include/ConfParser.h"
#include "config.h"

#include <iostream>
#include <fstream>
#include <functional>
#include <utility>
#include <filesystem>
#include <cassert>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <list>
#include <variant>
#include <memory>
#include <string>
#include <vector>

class LogSink;

enum class LogLevel
{
    Debug,
    Info,
    Status,
    Warning,
    Error,
    Fatal,
    Max
};

enum class SinkType
{
    Console,
    File
};

struct LogSinkInfo
{
    LogLevel level = LogLevel::Info;
    std::string tag = "";
    LogLevel maxLevel = LogLevel::Fatal;
    std::mutex logMutex;
};

// Callers shouldn't worry about this, but a log stream is essentially a unix pipe.
// The return value is a file descriptor for the write end of the pipe
using LogStream = int;

class Log
{
  public:
    Log() = default;
    ~Log() = default;

    void LogAt (const std::string& message, LogLevel level) noexcept;
    // Only log message to sinks with the given tag. If no sinks have that tag, nothing will be
    // logged
    void LogWithTag (SinkType type, const std::string& message, LogLevel level) noexcept;
    void Fatal (const std::string& message) noexcept
    {
        LogAt (message, LogLevel::Fatal);
    }
    void Error (const std::string& message) noexcept
    {
        LogAt (message, LogLevel::Error);
    }
    void Warning (const std::string& message) noexcept
    {
        LogAt (message, LogLevel::Warning);
    }
    void Info (const std::string& message) noexcept
    {
        LogAt (message, LogLevel::Info);
    }
    void Status (const std::string& message) noexcept
    {
        LogAt (message, LogLevel::Status);
    }
    void Debug (const std::string& message) noexcept
    {
        LogAt (message, LogLevel::Debug);
    }

    void SetSinkLogLevel (SinkType type, LogLevel level);
    // Useful for disabling logging across the board
    void SetLogLevel (LogLevel level)
    {
        this->level = level;
    }
    // Sets up a stream for all sinks with the specified type. All messages written to stream will
    // be logged to those sinks. Returns file descriptor for write end of the pipe
    Result<LogStream> StreamIntoSink (SinkType type,
                                      const std::string& msgPrefix,
                                      LogLevel level = LogLevel::Debug);

    // There is no function for managed logging, as that is meant to be invisible to the program. It
    // is always on, and all gets logged to it
    ResNone AddFileSink (LogSinkInfo& info, const std::string& filename);
    ResNone AddConsoleSink (LogSinkInfo& info, std::ostream& out);

  private:
    LogLevel level = LogLevel::Info;    // This is the default log level for all sinks
    std::vector<LogStream> logStreams;
    std::vector<std::pair<std::unique_ptr<LogSink>, LogSinkInfo>> sinks;
};

extern std::unique_ptr<Log> _log;

class LogSink
{
  public:
    LogSink() = default;
    virtual ~LogSink() = default;

    virtual void Log (const std::string& message, LogLevel level) noexcept = 0;

  protected:
    LogLevel minLevel;
    LogLevel maxLevel;
};

// Struct that represent parameters at each log level
struct ConsLogParams
{
    std::string prefix;
    std::string color;
    bool printPrefix = true;
};

// Log sink types
// There aren't many, so we'll just keep them here
class ConsoleLogSink : public LogSink
{
  public:
    ConsoleLogSink (const char* progName, std::ostream& out);
    ~ConsoleLogSink() override = default;

    void Log (const std::string& message, LogLevel level) noexcept override;

  private:
    bool checkIsOutColor() const;

    const char* progName;
    bool isOutColor = false;
    std::ostream& out;
    std::string ansiReset = ANSI_CODE_RESET;

    inline static const EnumArray<LogLevel, ConsLogParams, LogLevel::Max> logParams = {
        {LogLevel::Debug, {"note: ", ANSI_CODE_INFO, true}},
        {LogLevel::Info, {"info: ", ANSI_CODE_INFO, true}},
        {LogLevel::Status, {"", "", false}},
        {LogLevel::Warning, {"warning: ", ANSI_CODE_WARN, true}},
        {LogLevel::Error, {"error: ", ANSI_CODE_ERROR, true}},
        {LogLevel::Fatal, {"fatal error: ", ANSI_CODE_ERROR, true}}};
};

class FileLogSink : public LogSink
{
  public:
    FileLogSink (const std::string& filename);
    ~FileLogSink() override;

    void Log (const std::string& message, LogLevel level) noexcept override;

  private:
    std::ofstream file;
    // TODO: maybe dedupe this with ConsoleLogSink?
    inline static const EnumArray<LogLevel, std::string, LogLevel::Max> logParams = {
        {LogLevel::Debug, "note: "},
        {LogLevel::Info, "info: "},
        {LogLevel::Status, ""},
        {LogLevel::Warning, "warning: "},
        {LogLevel::Error, "error: "},
        {LogLevel::Fatal, "fatal error: "}};
};

constexpr int MAX_LOG_COUNT = 50;

class ManagedLogSink : public LogSink
{
  public:
    ManagedLogSink();
    ~ManagedLogSink() override = default;

    void Log (const std::string& message, LogLevel level) noexcept override;

  private:
    std::string getLogFileName();
    ResNone loadCtrlFile (std::filesystem::path ctrlPath);

    std::filesystem::path ctrlPath;
    LockFile logCtrlLock;

    const std::string filePrefix = "nnimage_log_";
    int curLogCount = 0;
    std::ofstream curLog;
    std::filesystem::path logDir;

    inline static const EnumArray<LogLevel, std::string, LogLevel::Max> logParams = {
        {LogLevel::Debug, "[NOTE] "},
        {LogLevel::Info, "[INFO] "},
        {LogLevel::Status, "[STATUS] "},
        {LogLevel::Warning, "[WARNING] "},
        {LogLevel::Error, "[ERROR] "},
        {LogLevel::Fatal, "[FATAL] "}};
};

// Managed log controller. Represents the file nnimage_logctrl
enum class LogCtrlKey
{
    None,
    FileCount,
    Files,
    Max
};

// NOTE: MUST BE SYNCED WITH ORDER OF LogCtrlValue
// I know this is the C programmer coming out in me, but this trickery is the cleanest way to do
// this without a lot of template boilerplate
enum class LogCtrlType
{
    Int,
    String,
    List,
    Max
};

// KEEP SYNCED WITH ABOVE
using LogCtrlValue = std::variant<int, std::string, std::vector<std::string>>;
using LogList = std::vector<std::string>;

class ManagedLogCtrl;
using LogCtrlSetter = std::function<void (ManagedLogCtrl*, const LogCtrlValue&)>;
using LogCtrlGetter = std::function<LogCtrlValue (ManagedLogCtrl*)>;

struct LogCtrlInstance
{
    LogCtrlType type;
    LogCtrlSetter setter;
    LogCtrlGetter getter;
};

struct LogCtrlProp
{
    std::string name;
    LogCtrlValue val;
};

using TokenPtr = std::unique_ptr<LexToken>;

class ManagedLogCtrl : public ConfParser<ManagedLogCtrl, LogCtrlKey>
{
  public:
    ManagedLogCtrl (std::ifstream& file);
    ~ManagedLogCtrl();

  private:
    // Data fields. We just store each value as a class member as this is a very simple config file
    // and something fancy would be overkill
    std::vector<std::string> logFiles;
    int fileCount;

    static const EnumArray<LogCtrlKey, LogCtrlInstance, LogCtrlKey::Max> keys;
    inline static const std::string logCtrlFile = "nnimage_logctrl";
    // Table to convert property names to keys, so we can get to the setter
    inline static const std::unordered_map<std::string, LogCtrlKey> nameToKey = {
        {"max_file", LogCtrlKey::FileCount},
        {"log_files", LogCtrlKey::Files}};
};

#endif
