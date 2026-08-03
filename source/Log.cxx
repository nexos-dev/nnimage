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
#include <thread>
#include <iostream>
#include <sys/types.h>
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

// Main interface
void Log::Error (const std::string& msg)
{
    LogAt (msg, LogLevel::Error);
}

void Log::Warn (const std::string& msg)
{
    LogAt (msg, LogLevel::Warning);
}

void Log::Info (const std::string& msg)
{
    LogAt (msg, LogLevel::Info);
}

void Log::SysError (const std::string& msg)
{
    LogAt (msg + ": " + std::string (strerror (errno)), LogLevel::Error);
}

// Table of loglevel to message prefix
static const char* logLevelPrefix[] = {"error: ", "warning: ", "", ""};
static const char* ansicodePrefix[] = {ANSI_CODE_ERROR, ANSI_CODE_WARN, "", ""};
static bool isLevelCerr[] = {true, true, false, false};

// Called with mutex locked
void Log::logToCerr (const std::string& msg, LogLevel level)
{
    int levelInt = static_cast<int> (level);
    assert (levelInt >= 0 && levelInt < 4);
    const char* prefix = logLevelPrefix[levelInt];
    std::cerr << this->prog << ": ";
    if (this->isCerrTty)
        std::cerr << ansicodePrefix[levelInt] << prefix << ANSI_CODE_RESET;
    else
        std::cerr << prefix;
    std::cerr << msg << "\n";
}

// Called with mutex locked
void Log::logToCout (const std::string& msg, LogLevel level)
{
    const char* prefix = logLevelPrefix[static_cast<int> (level)];
    std::cout << this->prog << ": ";
    if (this->isCoutTty)
        std::cout << ANSI_CODE_ERROR << prefix << ANSI_CODE_RESET;
    else
        std::cout << prefix;
    std::cout << msg << "\n";
}

void Log::LogAt (const std::string& msg, LogLevel level)
{
    std::lock_guard<std::mutex> guard (logMtx);
    if (level <= logLevel)
    {
        if (isLogEnabled.load())
        {
            if (isLevelCerr[static_cast<int> (level)])
                logToCerr (msg, level);
            else
                logToCout (msg, level);
        }
    }
    // Always log to files
    addLine (msg, level);
}

void Log::SetLogLevel (LogLevel level)
{
    logLevel = level;
}

void Log::Disable()
{
    isLogEnabled = false;
}

void Log::Enable()
{
    isLogEnabled = true;
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
    // Cleanup every stream
    for (int fd : logStreams)
    {
        close (fd);
    }
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

int Log::CreateLogStream (const std::string& msgPrefix, LogLevel level)
{
    // Create a pipe
    int pipeFd[2];
    if (pipe (pipeFd) == -1)
    {
        SysError ("failed to create pipe for log stream");
        return -1;
    }
    logStreams.push_back (pipeFd[0]);
    logStreams.push_back (pipeFd[1]);
    // Create a thread to read from the pipe and log the output
    std::thread logThread ([this, level, pipeFd, msgPrefix]() {
        // Prepare output buffer
        char* buffer = (char*) malloc (1);
        size_t bufSize = 1;
        // Temporary buffer for reading from pipe
        char tmp[1024];
        ssize_t bytesRead = read (pipeFd[0], tmp, sizeof (tmp));
        // Keep reading until we reach EOF
        while (bytesRead > 0)
        {
            // Increase buffer size
            buffer = (char*) realloc (buffer, bufSize + bytesRead);
            memcpy (buffer + bufSize - 1, tmp, bytesRead);
            bufSize += bytesRead;
            // Null terminate
            buffer[bufSize - 1] = '\0';
            // Read again
            bytesRead = read (pipeFd[0], tmp, sizeof (tmp));
        }
        if (bytesRead == -1)
            SysError ("failed to read from log stream");
        // If data is in the buffer, log it
        else if (bufSize > 1)
        {
            std::string msg = msgPrefix + std::string (buffer);
            LogAt (msg, level);
        }
        free (buffer);
    });
    logThread.detach();
    return pipeFd[1];
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

// NOTE: must be called with mutex locked
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
