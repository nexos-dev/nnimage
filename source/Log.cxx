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

#include "nnimage.h"
#include <unistd.h>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cerrno>

void Log::LogAt (const std::string& message, LogLevel level) noexcept
{}

// Log sink implementations
FileLogSink::FileLogSink (const std::string& filename) : file (filename)
{
    if (!file.is_open())
    {
        throw ErrorException (
            Error ({ErrorDomain::Log, ErrorCode::FileError}, "Failed to open log file: " + filename)
                .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true));
    }
}
FileLogSink::~FileLogSink()
{
    if (file.is_open())
        file.close();
}

void FileLogSink::Log (const std::string& message, LogLevel level) noexcept
{
    file << logParams[level] << message << "\n";
    file.flush();
}

ConsoleLogSink::ConsoleLogSink (const char* progName, std::ostream& out)
    : out (out), progName (progName)
{
    // Check if this is a color terminal
    isOutColor = checkIsOutColor();
    if (!isOutColor)
        _log->Debug ("Output stream is not a color terminal, disabling color output");
}

void ConsoleLogSink::Log (const std::string& message, LogLevel level) noexcept
{
    // Grab our parameters for this log level
    const ConsLogParams& params = logParams[level];
    std::string output = "";
    if (params.printPrefix)
        output = progName + std::string (": ") + params.color + params.prefix + ansiReset;
    output += message + "\n";
    out << output;
    out.flush();
}

bool ConsoleLogSink::checkIsOutColor() const
{
    // First check if color is forced
    if (getenv ("FORCE_COLOR") != nullptr || getenv ("NNIMAGE_FORCE_COLOR") != nullptr)
        return true;
    else if (getenv ("NO_COLOR") != nullptr)
        return false;
    // Check if the output stream is cerr or cout
    int unixFd = 0;
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
    bool isColor = termStr.find ("color") != std::string::npos ||
                   termStr.find ("256") != std::string::npos ||
                   termStr.find ("xterm") != std::string::npos;
    return isColor;
}

ManagedLogSink::ManagedLogSink()
{
    logCtrlLock = LockFile ("test");
}

ResNone ManagedLogSink::loadCtrlFile (std::filesystem::path ctrlPath)
{}

ManagedLogCtrl::ManagedLogCtrl (std::ifstream& file) : ConfParser<ManagedLogCtrl, LogCtrlKey> (file)
{
    if (!std::filesystem::exists (logDir))
    {
        _log->Debug ("Log directory does not exist, creating: " + logDir.string());
        std::filesystem::create_directories (logDir);
    }
    else if (!std::filesystem::is_directory (logDir))
    {
        throw ErrorException (Error ({ErrorDomain::Log, ErrorCode::FileError},
                                     "Log path is not a directory: " + logDir.string()));
    }
    // Now grab the log ctrl file
    std::filesystem::path logCtrlPath = logDir / logCtrlFile;
    auto res = loadFile (logCtrlPath);
    if (!res.IsOk())
        throw ErrorException (res.GetError());
    confPath = logCtrlPath.string();
}

ManagedLogCtrl::~ManagedLogCtrl()
{}

ResNone ManagedLogCtrl::loadFile (std::filesystem::path ctrlPath)
{
    std::string confFile;
    if (!std::filesystem::exists (ctrlPath))
    {
        // Create the log control file
        std::ofstream ctrlFile (ctrlPath);
        if (!ctrlFile.is_open())
        {
            return Error ({ErrorDomain::Log, ErrorCode::FileError},
                          "Failed to create log control file: " + ctrlPath.string())
                .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true);
        }
        ctrlFile.close();
        confFile = "";    // Ensure that ctrlFile is empty, since we just created the
                          // log control file
    }
    else
    {
        std::ifstream ctrlFile (ctrlPath, std::ios::binary | std::ios::ate);
        if (!ctrlFile.is_open())
        {
            return Error ({ErrorDomain::Log, ErrorCode::FileError},
                          "Failed to open log control file: " + ctrlPath.string())
                .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true);
        }
        // Read the file into a string
        std::streamsize size = ctrlFile.tellg();
        ctrlFile.seekg (0, std::ios::beg);
        confFile.resize (size);
        if (!ctrlFile.read (confFile.data(), size))
        {
            return Error ({ErrorDomain::Log, ErrorCode::FileError},
                          "Failed to read log control file: " + ctrlPath.string())
                .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true);
        }
        ctrlFile.close();
    }
    return Success();
}

// Log control table array
const EnumArray<LogCtrlKey, LogCtrlInstance, LogCtrlKey::Max> ManagedLogCtrl::keys = {
    {LogCtrlKey::FileCount,
     {LogCtrlType::Int,
      [] (ManagedLogCtrl* ctrl, const LogCtrlValue& val) {
          assert (ctrl && std::holds_alternative<int> (val));
          ctrl->fileCount = std::get<int> (val);
      },
      [] (ManagedLogCtrl* ctrl) {
          assert (ctrl);
          return LogCtrlValue (ctrl->fileCount);
      }}},
    {LogCtrlKey::Files,
     {LogCtrlType::List,
      [] (ManagedLogCtrl* ctrl, const LogCtrlValue& val) {
          assert (ctrl && std::holds_alternative<LogList> (val));
          ctrl->logFiles = std::get<LogList> (val);
      },
      [] (ManagedLogCtrl* ctrl) { return LogCtrlValue (ctrl->logFiles); }}}};
