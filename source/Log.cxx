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
#include <errno.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <sstream>
#include <string.h>
#include <time.h>
#include <unistd.h>

Log::Log (const char* progName, LogLevel defaultLevel)
{
    this->prog = progName;
    // Check if cout or cerr is a tty
    if (isatty (STDOUT_FILENO))
        this->isCoutTty = true;
    else
        this->isCoutTty = false;
    if (isatty (STDERR_FILENO))
        this->isCerrTty = true;
    else
        this->isCerrTty = false;
    // Set default log level
    if (defaultLevel <= LogLevel::Verbose)
        this->logLevel = defaultLevel;
    // Set start time
    logStartTime = time (NULL);
    // Make the name
    std::string logFile = getLogPath (LOG_FILE);
    std::ofstream log (logFile);
    if (!log.is_open())
        Warn ("unable to open log file");    // Error, but not fatal
    else
        logs.push_back (std::move (log));
}

void Log::Error (const std::string& msg)
{
    std::cerr << this->prog << ": ";
    // Check if we are using color or not
    if (this->isCerrTty)
        std::cerr << ANSI_CODE_ERROR << "error: " << ANSI_CODE_RESET;
    else
        std::cerr << "error: ";
    std::cerr << msg << "\n";
    // Now add to line list
    addLine (msg, LogLevel::Error);
}

void Log::Warn (const std::string& msg)
{
    if (logLevel > LogLevel::Warning)
        return;
    std::cerr << this->prog << ": ";
    // Check if we are using color or not
    if (this->isCerrTty)
        std::cerr << ANSI_CODE_WARN << "warning: " << ANSI_CODE_RESET;
    else
        std::cerr << "warning: ";
    std::cerr << msg << "\n";
    // Now add to line list
    addLine (msg, LogLevel::Warning);
}

void Log::Info (const std::string& msg)
{
    if (logLevel > LogLevel::Info)
        return;
    std::cout << msg << "\n";
    // Now add to line list
    addLine (msg, LogLevel::Info);
}

void Log::SysError (const std::string& msg)
{
    std::cerr << this->prog << ": ";
    // Check if we are using color or not
    if (this->isCerrTty)
        std::cerr << ANSI_CODE_ERROR << "error: " << ANSI_CODE_RESET;
    else
        std::cerr << "error: ";
    std::cerr << msg << ": " << strerror (errno) << "\n";
    // Now add to line list
    addLine (msg, LogLevel::Error);
}

void Log::SetLogLevel (LogLevel level)
{
    logLevel = level;
}

Log::~Log()
{
    // Go through every log file
    for (std::ofstream& buf : logs)
    {
        for (const std::string& msg : lines)
        {
            buf << msg;
        }
        buf.close();
    }
    // TODO: cleanup old logs
}

bool Log::AddFile (const std::string& fileName)
{
    // Create the ostream
    std::ofstream output (fileName);
    if (!output.is_open())
    {
        Error ("unable to open log file \"" + fileName + "\'");
        return false;
    }
    logs.push_back (std::move (output));
    return true;
}

const std::string Log::getLogPath (const std::string& logName)
{
    // Get user's home
    const char* userHome = getenv ("HOME");
    assert (userHome);
    std::string log = userHome;
    log += "/.local/share/";
    log += logName;
    // Now create log directories
    std::error_code ec;
    std::filesystem::path logPath = log;
    std::filesystem::create_directories (logPath.parent_path(), ec);
    if (ec)
        return {};
    return log;
}

void Log::addLine (const std::string& line, LogLevel level)
{
    // First figure out time since running this occured
    time_t now;
    time (&now);
    time_t msgTime = now - logStartTime;
    std::ostringstream msg;
    msg << "[" << std::setw (6) << msgTime << "]";
    if (level == LogLevel::Error)
        msg << "error: ";
    else if (level == LogLevel::Warning)
        msg << "warning: ";
    msg << line;
    msg << "\n";
    lines.push_back (msg.str());
}
