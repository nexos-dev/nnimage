/*
    Log.cxx - contains log implementation
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

#include "include/Log.h"
#include "include/sys/LockFile.h"
#include "include/sys/TextReader.h"

#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

Log::~Log() = default;
Log::Log() = default;

void Log::LogAt (std::string_view message, LogLevel level, LogTime time)
{
    if (time == LogTime{})
        time = std::chrono::system_clock::now();
    std::shared_lock lock (sinkMtx);

    for (auto& sink : sinks)
    {
        auto& sinkInfo = sink.second;
        if (!isLoggable (sinkInfo, level))
            continue;

        auto lock = sink.first->LockSink();
        sink.first->Log (message, level, time);
    }
}

void Log::LogWithTag (SinkType type, std::string_view message, LogLevel level, LogTime time)
{
    if (time == LogTime{})
        time = std::chrono::system_clock::now();
    std::shared_lock lock (sinkMtx);

    for (auto& sink : sinks)
    {
        if (sink.second.type == type)
        {
            auto& sinkInfo = sink.second;
            if (!isLoggable (sinkInfo, level))
                continue;

            auto lock = sink.first->LockSink();
            sink.first->Log (message, level, time);
        }
    }
}

void Log::SetSinkLogLevel (SinkType type, LogLevel level)
{
    std::unique_lock lock (sinkMtx);
    for (auto& sink : sinks)
    {
        if (sink.second.type == type)
        {
            auto lock = sink.first->LockSink();
            sink.second.level = level;
        }
    }
}

void Log::SetSinkMaxLogLevel (SinkType type, LogLevel level)
{
    std::unique_lock lock (sinkMtx);
    for (auto& sink : sinks)
    {
        if (sink.second.type == type)
        {
            auto lock = sink.first->LockSink();
            sink.second.maxLevel = level;
        }
    }
}

Result<LogStream> Log::StreamIntoSink (SinkType type, std::string_view msgPrefix, LogLevel level)
{
    return -1;
}

ResNone Log::AddFileSink (LogSinkInfo& info, const std::filesystem::path& filename)
{
    // Validate the sink info
    assert (info.maxLevel >= info.level);
    info.type = SinkType::File;
    std::unique_ptr<FileLogSink> sink;
    try
    {
        sink = std::make_unique<FileLogSink> (filename);
    }
    catch (ErrorException& e)
    {
        return e.Error();
    }
    // Add it
    std::unique_lock<std::shared_mutex> lock (sinkMtx);
    sinks.emplace_back (std::move (sink), info);
    return Success();
}

ResNone Log::AddConsoleSink (LogSinkInfo& info, const char* progName, std::ostream& out)
{
    assert (info.maxLevel >= info.level);
    info.type = SinkType::Console;
    auto sink = std::make_unique<ConsoleLogSink> (progName, out);

    std::unique_lock<std::shared_mutex> lock (sinkMtx);
    sinks.emplace_back (std::move (sink), info);
    return Success();
}

ResNone Log::AddManagedSink (LogSinkInfo& info, std::filesystem::path logDir)
{
    assert (info.maxLevel >= info.level);
    info.type = SinkType::Managed;

    std::unique_ptr<ManagedLogSink> sink;
    try
    {
        sink = std::make_unique<ManagedLogSink> (logDir);
    }
    catch (ErrorException& e)
    {
        return e.Error();
    }

    std::unique_lock<std::shared_mutex> lock (sinkMtx);

    auto res = sink->Prepare();
    if (!res)
        return res;

    sinks.emplace_back (std::move (sink), info);
    return Success();
}

// Log sink implementations
FileLogSink::FileLogSink (const std::filesystem::path& filename) : file (filename)
{
    if (!file.is_open())
    {
        throw ErrorException (Error ({ErrorDomain::Log, ErrorCode::LogFileOpen}, {{"file", filename.string()}})
                .Add ({ErrorDomain::Log, ErrorCode::SysFailure, ErrorLog::Debug}, {{"error", std::strerror (errno)}}));
    }
}
FileLogSink::~FileLogSink()
{
    if (file.is_open())
        file.close();
}

void FileLogSink::Log (std::string_view message, LogLevel level, LogTime time)
{
    file << logParams[level] << message << "\n";
    if (level >= LogLevel::Error)
        file.flush();
}

ConsoleLogSink::ConsoleLogSink (const char* progName, std::ostream& out) : out (out), progName (progName)
{
    // Check if this is a color terminal
    isOutColor = checkIsOutColor();
    if (!isOutColor)
        Log::The().Debug ("Output stream is not a color terminal, disabling color output");
}

void ConsoleLogSink::Log (std::string_view message, LogLevel level, LogTime time)
{
    // Grab our parameters for this log level
    const ConsLogParams& params = logParams[level];
    if (params.printPrefix && isOutColor)
        out << progName << ": " << params.color << params.prefix << ansiReset;
    else
        out << progName << ": " << params.prefix;
    out << message << std::endl;
}

bool ConsoleLogSink::checkIsOutColor() const
{
    // First check if color is forced
    if (getenv ("FORCE_COLOR") != nullptr || getenv ("NNIMAGE_FORCE_COLOR") != nullptr)
        return true;
    else if (getenv ("NO_COLOR") != nullptr)
        return false;

    // Check if the output stream is cerr or cout
    int unixFd = -1;
    if (&out == &std::cout)
        unixFd = STDOUT_FILENO;
    else if (&out == &std::cerr)
        unixFd = STDERR_FILENO;
    else
        return false;    // Don't bother guessing, we have no way of knowing what the unix FD is
                         // without using horrible hacks
                         // TODO: maybe we just use C streams in the first place?
    if (!isatty (unixFd))
        return false;

    // Now check term
    const char* term = getenv ("TERM");
    if (term == nullptr)
        return false;
    std::string termStr (term);
    bool isColor = termStr.find ("color") != std::string::npos || termStr.find ("256") != std::string::npos ||
                   termStr.find ("xterm") != std::string::npos;
    return isColor;
}

ManagedLogSink::ManagedLogSink (std::filesystem::path logDir) : logDir{logDir}
{
    if (!std::filesystem::exists (logDir))
    {
        if (!std::filesystem::create_directories (logDir))    // Go ahead and create it
        {
            throw ErrorException (Error ({ErrorDomain::Log, ErrorCode::LogPathCreate}, {}));
        }
    }
    else if (!std::filesystem::is_directory (logDir))
    {
        throw ErrorException (Error ({ErrorDomain::Log, ErrorCode::LogPathNotDirectory}, {}));
    }
    ctrlPath = logDir / logCtrlFile;
}

void ManagedLogSink::Log (std::string_view message, LogLevel level, LogTime time)
{
    curLog << "[" << Timestamp::MakeTimestampNow().View() << "]" << logParams[level] << message << "\n";
    if (level >= LogLevel::Error)
        curLog.flush();
}

ResNone ManagedLogSink::Prepare()
{
    std::string name = getLogName();
    std::filesystem::path log = logDir / name;

    // TODO: racey
    int i = 0;
    while (std::filesystem::exists (log))
    {
        log = logDir / (name + "_" + std::to_string (i));
        i++;
    }

    curLog = std::ofstream (log, std::ios::trunc);
    if (!curLog.is_open())
        return Error ({ErrorDomain::Log, ErrorCode::ManagedLogOpen}, {});

    // Create log worker
    maintThread = std::jthread (logMaintWorker, std::ref (*this));

    return Success();
}

ResNone ManagedLogSink::openLogCtrl (ManagedLogSink& inst, int& maxAge, int& maxLogs)
{
    std::string data;
    try
    {
        TextReader ctrlReader = TextReader (inst.ctrlPath);
        auto res = ctrlReader.Read();
        if (!res)
            return res.Error();
        data = std::move (res.Value());
    }
    catch (ErrorException& e)
    {
        return e.Error();
    }

    ManagedLogCtrl logCtrl = ManagedLogCtrl (inst.ctrlPath.string(), data);
    auto resParse = logCtrl.Parse();
    if (!resParse)
        return resParse.Error();

    // Get our values
    ConfValue val;
    auto res = logCtrl.Get (LogCtrlKey::MaxFiles, val);
    if (res.Value())
        maxLogs = std::get<int> (val);
    res = logCtrl.Get (LogCtrlKey::MaxAge, val);
    if (res.Value())
        maxAge = std::get<int> (val);
    assert (maxAge >= 0 && maxLogs >= 0);
    return Success();
}

bool ManagedLogSink::checkLog (const std::filesystem::path& log, int maxAge)
{
    auto writeTime = std::filesystem::last_write_time (log);
    auto lastWrite = std::chrono::file_clock::to_sys (writeTime);
    auto timeNow = std::chrono::system_clock::now();

    auto nowDays = std::chrono::floor<std::chrono::days> (timeNow);
    auto writeDays = std::chrono::floor<std::chrono::days> (lastWrite);

    if ((nowDays.time_since_epoch().count() - writeDays.time_since_epoch().count()) > maxAge)
    {
        std::filesystem::remove (log);
        return true;
    }
    return false;
}

void ManagedLogSink::deleteOldestLogs (std::vector<std::filesystem::path>& files, int count)
{
    // First we need to sort by oldest
    std::sort (files.begin(), files.end(), [] (const auto& a, const auto& b) {
        return std::filesystem::last_write_time (a) < std::filesystem::last_write_time (b);
    });

    // Now delete count
    for (int i = 0; i < count; i++)
    {
        auto& path = files[i];
        std::filesystem::remove (path);
    }
}

void ManagedLogSink::logMaintWorker (ManagedLogSink& inst)
{
    int maxAge = MAX_LOG_AGE, maxLogs = MAX_LOG_COUNT;

    // First parse the control
    if (std::filesystem::exists (inst.ctrlPath))
    {
        auto res = openLogCtrl (inst, maxAge, maxLogs);
        if (!res)
        {
            ErrorOutput::The()->Report (res.Error().Add ({ErrorDomain::Log, ErrorCode::ManagedLogControlOpen}, {}));
            return;
        }
    }

    // Now we have the parameters, it's time to begin checking logs
    std::filesystem::path lockPath = inst.logDir / "logmaint_lock";
    auto res = LockFileUnique::TryAcquire (lockPath);
    if (!res.has_value())
        return;    // We were beaten to it
    auto lock = std::move (res.value());

    // Pass 1: delete every out-of-date file
    int numLogs = 0;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator (inst.logDir))
    {
        // Check for prefix
        std::string file = entry.path().filename().string();
        if (file.substr (0, filePrefix.length()) == filePrefix && entry.is_regular_file())
        {
            if (!checkLog (entry.path(), maxAge))
            {
                files.push_back (entry.path());
                numLogs++;    // Do it now so we don't have to iterate again later
            }
        }
    }

    // Pass 2: delete oldest files if we have more than the max
    if (numLogs > maxLogs)
        deleteOldestLogs (files, numLogs - maxLogs);
}

// Log control table array
const EnumArray<LogCtrlKey, ConfInstance<ManagedLogCtrl, LogCtrlKey>, LogCtrlKey::Max> ManagedLogCtrl::keys = {
    {LogCtrlKey::MaxFiles,
        {ConfType::Int,
            [] (ManagedLogCtrl& ctrl, const ConfValue& val) {
                assert (std::holds_alternative<int> (val));
                ctrl.maxLogs = std::get<int> (val);
            },
            [] (ManagedLogCtrl& ctrl) -> ConfValue {
                int count = ctrl.maxLogs;
                if (count == -1)
                    return std::monostate{};
                return count;
            }}},
    {LogCtrlKey::MaxAge,
        {ConfType::Int,
            [] (ManagedLogCtrl& ctrl, const ConfValue& val) {
                assert (std::holds_alternative<int> (val));
                ctrl.maxAge = std::get<int> (val);
            },
            [] (ManagedLogCtrl& ctrl) -> ConfValue {
                int count = ctrl.maxAge;
                if (count == -1)
                    return std::monostate{};
                return count;
            }}}};
