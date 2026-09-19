/*
    Log.cxx - contains log test cases
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

#include "doctest.h"
#include "include/Log.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

// Helper to make a unique scratch directory per test so parallel/rerun runs don't collide
static std::filesystem::path MakeScratchDir (const std::string& tag)
{
    auto dir = std::filesystem::temp_directory_path() /
               ("nnimage_log_test_" + tag + "_" + std::to_string (reinterpret_cast<uintptr_t> (&tag)));
    std::filesystem::remove_all (dir);
    std::filesystem::create_directories (dir);
    return dir;
}

static std::string ReadFile (const std::filesystem::path& path)
{
    std::ifstream in (path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/********************
 *
 * Log test cases
 *
 *********************/

TEST_CASE ("Log console sink respects per-sink and global log levels")
{
    Log log;
    std::ostringstream out;
    LogSinkInfo info;
    info.level = LogLevel::Warning;
    info.maxLevel = LogLevel::Fatal;
    CHECK (log.AddConsoleSink (info, "prog", out));

    log.Info ("should be filtered");
    CHECK (out.str().empty());

    log.Warning ("should show");
    CHECK (out.str().find ("should show") != std::string::npos);
}

TEST_CASE ("Log respects a maxLevel ceiling on a sink")
{
    Log log;
    std::ostringstream out;
    LogSinkInfo info;
    info.level = LogLevel::Debug;
    info.maxLevel = LogLevel::Warning;
    CHECK (log.AddConsoleSink (info, "prog", out));

    log.Fatal ("too severe for this sink");
    CHECK (out.str().empty());

    log.Warning ("within range");
    CHECK (out.str().find ("within range") != std::string::npos);
}

TEST_CASE ("Log::SetLogLevel suppresses everything below the global threshold regardless of sink config")
{
    Log log;
    std::ostringstream out;
    LogSinkInfo info;
    info.level = LogLevel::Debug;
    info.maxLevel = LogLevel::Fatal;
    CHECK (log.AddConsoleSink (info, "prog", out));

    log.SetLogLevel (LogLevel::Fatal);
    log.Error ("silenced by global level");
    CHECK (out.str().empty());

    log.Fatal ("passes global level");
    CHECK (out.str().find ("passes global level") != std::string::npos);
}

TEST_CASE ("Log::SetSinkLogLevel and SetSinkMaxLogLevel adjust filtering at runtime")
{
    Log log;
    std::ostringstream out;
    LogSinkInfo info;
    info.level = LogLevel::Info;
    info.maxLevel = LogLevel::Fatal;
    CHECK (log.AddConsoleSink (info, "prog", out));

    log.SetSinkLogLevel (SinkType::Console, LogLevel::Error);
    log.Warning ("now filtered out");
    CHECK (out.str().empty());

    log.SetSinkMaxLogLevel (SinkType::Console, LogLevel::Warning);
    log.Error ("now above the new ceiling");
    CHECK (out.str().empty());
}

TEST_CASE ("Log::LogWithTag only delivers to sinks of the matching type")
{
    Log log;
    std::ostringstream consoleOut;
    std::filesystem::path dir = MakeScratchDir ("tag");
    std::filesystem::path filePath = dir / "log.txt";

    LogSinkInfo consoleInfo;
    consoleInfo.level = LogLevel::Debug;
    consoleInfo.maxLevel = LogLevel::Fatal;
    CHECK (log.AddConsoleSink (consoleInfo, "prog", consoleOut));

    LogSinkInfo fileInfo;
    fileInfo.level = LogLevel::Debug;
    fileInfo.maxLevel = LogLevel::Fatal;
    CHECK (log.AddFileSink (fileInfo, filePath.string()));

    log.LogWithTag (SinkType::Console, "console only", LogLevel::Info);
    CHECK (consoleOut.str().find ("console only") != std::string::npos);

    std::filesystem::remove_all (dir);
}

TEST_CASE ("Log::AddFileSink writes prefixed lines and flushes on error severity")
{
    std::filesystem::path dir = MakeScratchDir ("file");
    std::filesystem::path filePath = dir / "log.txt";
    {
        Log log;
        log.SetLogLevel (LogLevel::Debug);    // The global threshold defaults to Info and would otherwise mask Debug
        LogSinkInfo info;
        info.level = LogLevel::Debug;
        info.maxLevel = LogLevel::Fatal;
        REQUIRE (log.AddFileSink (info, filePath.string()));

        log.Debug ("dbg message");
        log.Warning ("warn message");
        log.Error ("err message");
    }    // Log (and thus the FileLogSink) is destroyed here, closing the file

    std::string content = ReadFile (filePath);
    CHECK (content.find ("note: dbg message") != std::string::npos);
    CHECK (content.find ("warning: warn message") != std::string::npos);
    CHECK (content.find ("error: err message") != std::string::npos);

    std::filesystem::remove_all (dir);
}

TEST_CASE ("Log::AddFileSink reports an error when the file cannot be opened")
{
    Log log;
    LogSinkInfo info;
    info.level = LogLevel::Info;
    info.maxLevel = LogLevel::Fatal;
    auto res = log.AddFileSink (info, "/nonexistent_dir_xyz/impossible/log.txt");
    CHECK_FALSE (res);
    CHECK (res.Error().GetSeverity() != ErrorSeverity::Warning);
}

TEST_CASE ("Log supports multiple sinks of the same type independently")
{
    Log log;
    std::ostringstream outA;
    std::ostringstream outB;
    LogSinkInfo infoA;
    infoA.level = LogLevel::Debug;
    infoA.maxLevel = LogLevel::Fatal;
    LogSinkInfo infoB;
    infoB.level = LogLevel::Error;
    infoB.maxLevel = LogLevel::Fatal;

    CHECK (log.AddConsoleSink (infoA, "progA", outA));
    CHECK (log.AddConsoleSink (infoB, "progB", outB));

    log.Warning ("only for A");
    CHECK (outA.str().find ("only for A") != std::string::npos);
    CHECK (outB.str().empty());

    log.Error ("for both");
    CHECK (outA.str().find ("for both") != std::string::npos);
    CHECK (outB.str().find ("for both") != std::string::npos);
}

TEST_CASE ("Log stress test with many rapid log calls across levels")
{
    std::filesystem::path dir = MakeScratchDir ("stress");
    std::filesystem::path filePath = dir / "log.txt";
    constexpr int iterations = 2000;
    {
        Log log;
        log.SetLogLevel (LogLevel::Debug);    // The global threshold defaults to Info and would otherwise mask Debug
        LogSinkInfo info;
        info.level = LogLevel::Debug;
        info.maxLevel = LogLevel::Fatal;
        REQUIRE (log.AddFileSink (info, filePath.string()));

        for (int i = 0; i < iterations; i++)
        {
            switch (i % 4)
            {
                case 0:
                    log.Debug ("msg " + std::to_string (i));
                    break;
                case 1:
                    log.Info ("msg " + std::to_string (i));
                    break;
                case 2:
                    log.Warning ("msg " + std::to_string (i));
                    break;
                case 3:
                    log.Error ("msg " + std::to_string (i));
                    break;
            }
        }
    }

    std::string content = ReadFile (filePath);
    size_t lineCount = std::count (content.begin(), content.end(), '\n');
    CHECK (lineCount == static_cast<size_t> (iterations));

    std::filesystem::remove_all (dir);
}

TEST_CASE ("ManagedLogCtrl parses max_file and max_age from control file syntax")
{
    std::string data = "max_file = 12;\nmax_age = 7;\n";
    ManagedLogCtrl ctrl ("test_ctrl", data);
    REQUIRE (ctrl.Parse());

    ConfValue val;
    auto res = ctrl.Get (LogCtrlKey::MaxFiles, val);
    REQUIRE (res);
    CHECK (res.Value());
    CHECK (std::get<int> (val) == 12);

    res = ctrl.Get (LogCtrlKey::MaxAge, val);
    REQUIRE (res);
    CHECK (res.Value());
    CHECK (std::get<int> (val) == 7);
}

TEST_CASE ("ManagedLogCtrl reports a parse error for malformed control files")
{
    ManagedLogCtrl ctrl ("bad_ctrl", "max_file = ;\n");
    auto res = ctrl.Parse();
    CHECK_FALSE (res);
}

TEST_CASE ("ManagedLogCtrl reports a type mismatch when a key gets the wrong value type")
{
    ManagedLogCtrl ctrl ("bad_ctrl_type", "max_file = \"not_a_number\";\n");
    auto res = ctrl.Parse();
    CHECK_FALSE (res);
}

TEST_CASE ("Log::AddManagedSink creates the log directory and an initial log file")
{
    std::filesystem::path dir = MakeScratchDir ("managed");
    std::filesystem::remove_all (dir);    // AddManagedSink must create it from scratch
    {
        Log log;
        LogSinkInfo info;
        info.level = LogLevel::Debug;
        info.maxLevel = LogLevel::Fatal;
        REQUIRE (log.AddManagedSink (info, dir));
        CHECK (std::filesystem::exists (dir));

        log.Info ("hello managed log");

        bool foundLogFile = false;
        for (auto& entry : std::filesystem::directory_iterator (dir))
        {
            if (entry.path().filename().string().starts_with ("nnimage_log_"))
                foundLogFile = true;
        }
        CHECK (foundLogFile);
    }
    std::filesystem::remove_all (dir);
}

TEST_CASE ("Log::AddManagedSink fails when the target path exists and is not a directory")
{
    std::filesystem::path dir = MakeScratchDir ("managed_file");
    std::filesystem::path filePath = dir / "not_a_dir";
    {
        std::ofstream touch (filePath);
        touch << "x";
    }

    Log log;
    LogSinkInfo info;
    info.level = LogLevel::Info;
    info.maxLevel = LogLevel::Fatal;
    auto res = log.AddManagedSink (info, filePath);
    CHECK_FALSE (res);

    std::filesystem::remove_all (dir);
}

TEST_CASE ("ManagedLogSink maintenance deletes files older than max_age")
{
    std::filesystem::path dir = MakeScratchDir ("maint_age");

    // Pre-seed an old-looking log file that should be pruned by the maintenance worker
    std::filesystem::path oldLog = dir / "nnimage_log_old_entry";
    {
        std::ofstream touch (oldLog);
        touch << "stale";
    }
    auto oldTime = std::filesystem::file_time_type::clock::now() - std::chrono::hours (72);
    std::filesystem::last_write_time (oldLog, oldTime);

    // Control file forces aggressive age-based pruning
    {
        std::ofstream ctrl (dir / "nnimage_logctrl");
        ctrl << "max_file = 50;\nmax_age = 1;\n";
    }

    {
        Log log;
        LogSinkInfo info;
        info.level = LogLevel::Debug;
        info.maxLevel = LogLevel::Fatal;
        REQUIRE (log.AddManagedSink (info, dir));
        // Log destructs at end of scope, joining the maintenance jthread before we inspect the directory
    }

    CHECK_FALSE (std::filesystem::exists (oldLog));

    std::filesystem::remove_all (dir);
}
