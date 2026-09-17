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
#include "include/sys/Timestamp.h"
#include "include/ConfParser.h"
#include "config.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <ostream>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
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
    File,
    Managed
};

struct LogSinkInfo
{
    LogLevel level = LogLevel::Info;
    LogLevel maxLevel = LogLevel::Fatal;
    SinkType type;
};

using LogTime = std::chrono::time_point<std::chrono::system_clock>;

// Callers shouldn't worry about this, but a log stream is essentially a unix pipe.
// The return value is a file descriptor for the write end of the pipe
using LogStream = int;

class Log
{
  public:
    static Log& The()
    {
        static Log instance;
        return instance;
    }

    Log (const Log&) = delete;
    Log& operator= (const Log&) = delete;
    ~Log();
    Log();

    void LogAt (const std::string& message, LogLevel level, LogTime time = LogTime{});
    // Only log message to sinks with the given tag. If no sinks have that tag, nothing will be
    // logged
    void LogWithTag (SinkType type, const std::string& message, LogLevel level, LogTime time = LogTime{});

    void Fatal (const std::string& message)
    {
        LogAt (message, LogLevel::Fatal);
    }
    void Error (const std::string& message)
    {
        LogAt (message, LogLevel::Error);
    }
    void Warning (const std::string& message)
    {
        LogAt (message, LogLevel::Warning);
    }
    void Info (const std::string& message)
    {
        LogAt (message, LogLevel::Info);
    }
    void Status (const std::string& message)
    {
        LogAt (message, LogLevel::Status);
    }
    void Debug (const std::string& message)
    {
        LogAt (message, LogLevel::Debug);
    }

    void SetSinkLogLevel (SinkType type, LogLevel level);
    void SetSinkMaxLogLevel (SinkType type, LogLevel level);
    // Useful for disabling logging across the board
    void SetLogLevel (LogLevel level)
    {
        this->level = level;
    }

    // Sets up a stream for all sinks with the specified type. All messages written to stream will
    // be logged to those sinks. Returns file descriptor for write end of the pipe
    Result<LogStream> StreamIntoSink (SinkType type, const std::string& msgPrefix, LogLevel level = LogLevel::Debug);

    ResNone AddFileSink (LogSinkInfo& info, const std::string& filename);
    ResNone AddConsoleSink (LogSinkInfo& info, const char* progName, std::ostream& out);
    ResNone AddManagedSink (LogSinkInfo& info, std::filesystem::path logDir);

  private:
    bool isLoggable (const LogSinkInfo& sinkInfo, LogLevel level)
    {
        if (level < sinkInfo.level || level > sinkInfo.maxLevel || level < this->level)
            return false;
        return true;
    }

    std::atomic<LogLevel> level = LogLevel::Info;    // This is the default log level for all sinks
    std::mutex streamMtx;
    std::vector<LogStream> logStreams;
    std::shared_mutex sinkMtx;
    std::vector<std::pair<std::unique_ptr<LogSink>, LogSinkInfo>> sinks;
};

class LogSink
{
  public:
    LogSink() = default;
    virtual ~LogSink() = default;

    virtual void Log (const std::string& message, LogLevel level, LogTime time) = 0;
    std::unique_lock<std::mutex> LockSink()
    {
        return std::unique_lock<std::mutex> (sinkMtx);
    }

  protected:
    mutable std::mutex sinkMtx;
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

    void Log (const std::string& message, LogLevel level, LogTime time) override;

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
        {LogLevel::Fatal, {"fatal: ", ANSI_CODE_ERROR, true}}};
};

class FileLogSink : public LogSink
{
  public:
    FileLogSink (const std::string& filename);
    ~FileLogSink() override;

    void Log (const std::string& message, LogLevel level, LogTime time) override;

  private:
    std::ofstream file;
    // TODO: maybe dedupe this with ConsoleLogSink?
    inline static const EnumArray<LogLevel, std::string, LogLevel::Max> logParams = {{LogLevel::Debug, "note: "},
        {LogLevel::Info, "info: "},
        {LogLevel::Status, ""},
        {LogLevel::Warning, "warning: "},
        {LogLevel::Error, "error: "},
        {LogLevel::Fatal, "fatal: "}};
};

// Managed log controller. Represents the file nnimage_logctrl
enum class LogCtrlKey
{
    None,
    MaxFiles,
    MaxAge,
    Max
};

constexpr int MAX_LOG_COUNT = 50;
constexpr int MAX_LOG_AGE = 30;

class ManagedLogCtrl : public ConfParser<ManagedLogCtrl, LogCtrlKey>
{
  public:
    ManagedLogCtrl() = default;
    ManagedLogCtrl (const std::string& fileName, std::string_view data)
        : ConfParser<ManagedLogCtrl, LogCtrlKey> (fileName, data)
    {}
    void Log (const std::string& message, LogLevel level, LogTime time);

  protected:
    const EnumArray<LogCtrlKey, ConfInstance<ManagedLogCtrl, LogCtrlKey>, LogCtrlKey::Max>& getKeyRegistry()
    {
        return keys;
    }

    const std::unordered_map<std::string, LogCtrlKey>& getNameToKey()
    {
        return nameToKey;
    }

    int maxLogs = -1;
    int maxAge = -1;

    static const EnumArray<LogCtrlKey, ConfInstance<ManagedLogCtrl, LogCtrlKey>, LogCtrlKey::Max> keys;
    // Table to convert property names to keys, so we can get to the setter
    inline static const std::unordered_map<std::string, LogCtrlKey> nameToKey = {{"max_file", LogCtrlKey::MaxFiles},
        {"max_age", LogCtrlKey::MaxAge}};
};

class ManagedLogSink : public LogSink
{
  public:
    ManagedLogSink (std::filesystem::path logDir);
    ResNone Prepare();
    void Log (const std::string& message, LogLevel level, LogTime time) override;

  private:
    std::string getLogName()
    {
        Timestamp now = Timestamp::MakeTimestampNow();
        std::string name = filePrefix;
        name += now.View();
        return name;
    }
    static void logMaintWorker (ManagedLogSink& inst);
    static bool checkLog (const std::filesystem::path& log, int maxAge);
    static ResNone openLogCtrl (ManagedLogSink& inst, const std::filesystem::path& ctrl, int& maxAge, int& maxLogs);
    static void deleteOldestLogs (std::vector<std::filesystem::path>& files, int count);

    std::filesystem::path ctrlPath;

    inline static const std::string filePrefix = "nnimage_log_";
    std::ofstream curLog;
    std::filesystem::path logDir;

    std::jthread maintThread;

    inline static const std::string logCtrlFile = "nnimage_logctrl";
    inline static const EnumArray<LogLevel, std::string, LogLevel::Max> logParams = {{LogLevel::Debug, "[NOTE] "},
        {LogLevel::Info, "[INFO] "},
        {LogLevel::Status, "[STATUS] "},
        {LogLevel::Warning, "[WARNING] "},
        {LogLevel::Error, "[ERROR] "},
        {LogLevel::Fatal, "[FATAL] "}};
};

#endif
