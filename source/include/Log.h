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

#ifndef NNIMAGE_LOG_H
#define NNIMAGE_LOG_H

#include <atomic>
#include <ctime>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

enum class LogLevel
{
    Error,
    Warning,
    Info,
    Status,
    Verbose
};

class Log
{
  public:
    Log()
    {}
    Log (const char* progName, LogLevel defaultLevel);
    ~Log();
    void Error (const std::string& msg);
    void Warn (const std::string& msg);
    void Info (const std::string& msg);
    void Verbose (const std::string& msg);
    void Status (const std::string& msg);
    void SysError (const std::string& msg);
    bool AddFile (const std::string& fileName);
    void LogAt (const std::string& msg, LogLevel level);
    void Disable();
    void Enable();
    void SetLogLevel (LogLevel level);
    int CreateLogStream (const std::string& msgPrefix, LogLevel level = LogLevel::Error);

  private:
    std::vector<std::ofstream> logs;
    LogLevel logLevel;
    bool isCoutTty;
    bool isCerrTty;
    std::atomic<bool> isLogEnabled = true;
    std::mutex logMtx;
    std::list<std::string> lines;
    const char* prog;
    time_t logStartTime;
    void addLine (const std::string& line, LogLevel level);
    const std::string getLogPath (const std::string& logName);
    void logToCerr (const std::string& msg, LogLevel level);
    void logToCout (const std::string& msg, LogLevel level);
    std::vector<int> logStreams;
};

extern std::unique_ptr<Log> _log;

#endif
