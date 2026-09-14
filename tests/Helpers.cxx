/*
    Helpers.cxx - contains test cases for small helper classes (LockFile, Iconv, Chardet)
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
#include "include/sys/LockFile.h"
#include "include/sys/Iconv.h"
#include "include/sys/Chardet.h"
#include "config.h"

#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <string>

static std::filesystem::path MakeScratchDir (const std::string& tag)
{
    auto dir = std::filesystem::temp_directory_path() /
               ("nnimage_helpers_test_" + tag + "_" + std::to_string (reinterpret_cast<uintptr_t> (&tag)));
    std::filesystem::remove_all (dir);
    std::filesystem::create_directories (dir);
    return dir;
}

/********************
 *
 * LockFile test cases
 *
 *********************/

TEST_CASE ("LockFile creates the backing file if it does not exist")
{
    auto dir = MakeScratchDir ("lockfile_create");
    auto path = dir / "brand_new.lock";
    REQUIRE_FALSE (std::filesystem::exists (path));

    LockFile lock (path);
    CHECK (std::filesystem::exists (path));

    std::filesystem::remove_all (dir);
}

TEST_CASE ("LockFile throws ErrorException when the backing path cannot be opened")
{
    CHECK_THROWS_AS (LockFile (std::string ("/nonexistent_dir_xyz/impossible.lock")), ErrorException);
}

TEST_CASE ("LockFile read and write locks change IsLocked state and Unlock resets it")
{
    auto dir = MakeScratchDir ("lockfile_state");
    auto path = dir / "state.lock";

    LockFile lock (path);
    CHECK_FALSE (lock.IsLocked());

    CHECK (lock.WriteLock());
    CHECK (lock.IsLocked());

    lock.Unlock();
    CHECK_FALSE (lock.IsLocked());

    CHECK (lock.ReadLock());
    CHECK (lock.IsLocked());
    lock.Unlock();
    CHECK_FALSE (lock.IsLocked());

    std::filesystem::remove_all (dir);
}

TEST_CASE ("LockFile move constructor and move assignment transfer ownership")
{
    auto dir = MakeScratchDir ("lockfile_move");
    auto path = dir / "move.lock";

    LockFile original (path);
    REQUIRE (original.WriteLock());

    LockFile moved (std::move (original));
    CHECK (moved.IsLocked());
    // The moved-from object must not think it still holds a lock
    CHECK_FALSE (original.IsLocked());

    LockFile another (dir / "other.lock");
    another = std::move (moved);
    CHECK (another.IsLocked());

    another.Unlock();
    std::filesystem::remove_all (dir);
}

TEST_CASE ("LockFileShared::TryAcquire succeeds and holds a read lock for its lifetime")
{
    auto dir = MakeScratchDir ("lockfile_shared");
    auto path = dir / "shared.lock";

    auto shared = LockFileShared::TryAcquire (path);
    REQUIRE (shared.has_value());

    std::filesystem::remove_all (dir);
}

TEST_CASE ("LockFileUnique::TryAcquire succeeds and holds a write lock for its lifetime")
{
    auto dir = MakeScratchDir ("lockfile_unique");
    auto path = dir / "unique.lock";

    auto unique = LockFileUnique::TryAcquire (path);
    REQUIRE (unique.has_value());

    std::filesystem::remove_all (dir);
}

// The following tests exercise real inter-process lock contention. fcntl()-based advisory locks are
// tracked per-process, so a second LockFile in the *same* process/thread cannot be used to observe
// contention - a genuine child process is required.
TEST_CASE ("LockFile write lock blocks other processes until released")
{
    auto dir = MakeScratchDir ("lockfile_fork_write");
    auto path = dir / "contended.lock";

    int pipefd[2];
    REQUIRE (pipe (pipefd) == 0);

    pid_t pid = fork();
    REQUIRE (pid >= 0);

    if (pid == 0)
    {
        // Child process
        close (pipefd[0]);
        LockFile childLock (path);
        char result = childLock.WriteLock (false) ? '1' : '0';
        write (pipefd[1], &result, 1);
        // Blocking attempt - should succeed once the parent releases the lock
        result = childLock.WriteLock (true) ? '1' : '0';
        write (pipefd[1], &result, 1);
        close (pipefd[1]);
        _exit (0);
    }

    // Parent process
    close (pipefd[1]);
    LockFile parentLock (path);
    REQUIRE (parentLock.WriteLock());

    char firstResult = 0;
    REQUIRE (read (pipefd[0], &firstResult, 1) == 1);
    CHECK (firstResult == '0');    // Child must fail to acquire while parent holds the lock

    parentLock.Unlock();

    char secondResult = 0;
    REQUIRE (read (pipefd[0], &secondResult, 1) == 1);
    CHECK (secondResult == '1');    // Child succeeds once the parent releases the lock

    close (pipefd[0]);
    int status = 0;
    waitpid (pid, &status, 0);

    std::filesystem::remove_all (dir);
}

TEST_CASE ("LockFile read locks can be shared across processes but block a writer")
{
    auto dir = MakeScratchDir ("lockfile_fork_read");
    auto path = dir / "shared_contended.lock";

    int pipefd[2];
    REQUIRE (pipe (pipefd) == 0);

    pid_t pid = fork();
    REQUIRE (pid >= 0);

    if (pid == 0)
    {
        close (pipefd[0]);
        LockFile childLock (path);
        char readResult = childLock.ReadLock (false) ? '1' : '0';
        write (pipefd[1], &readResult, 1);
        char writeResult = childLock.WriteLock (false) ? '1' : '0';
        write (pipefd[1], &writeResult, 1);
        close (pipefd[1]);
        _exit (0);
    }

    close (pipefd[1]);
    LockFile parentLock (path);
    REQUIRE (parentLock.ReadLock());

    char readResult = 0;
    REQUIRE (read (pipefd[0], &readResult, 1) == 1);
    CHECK (readResult == '1');    // Shared read locks may coexist

    char writeResult = 0;
    REQUIRE (read (pipefd[0], &writeResult, 1) == 1);
    CHECK (writeResult == '0');    // A writer must not be able to join while a reader holds the lock

    close (pipefd[0]);
    int status = 0;
    waitpid (pid, &status, 0);

    std::filesystem::remove_all (dir);
}

/********************
 *
 * Iconv test cases
 *
 *********************/

TEST_CASE ("Iconv converts ASCII data through a UTF-8 round trip unchanged")
{
    Iconv conv ("UTF-8", "UTF-8");
    std::string out;
    CHECK (conv.Convert ("hello world", out));
    CHECK (out == "hello world");
}

TEST_CASE ("Iconv converts between UTF-8 and Latin-1")
{
    Iconv toLatin1 ("UTF-8", "ISO-8859-1");
    std::string latin1;
    // "café" in UTF-8
    std::string utf8 = "caf\xc3\xa9";
    REQUIRE (toLatin1.Convert (utf8, latin1));
    CHECK (latin1 == "caf\xe9");

    Iconv toUtf8 ("ISO-8859-1", "UTF-8");
    std::string backToUtf8;
    REQUIRE (toUtf8.Convert (latin1, backToUtf8));
    CHECK (backToUtf8 == utf8);
}

TEST_CASE ("Iconv rejects empty input")
{
    Iconv conv ("UTF-8", "UTF-8");
    std::string out;
    CHECK_FALSE (conv.Convert ("", out));
}

TEST_CASE ("Iconv construction with an invalid encoding name marks the object unusable")
{
    Iconv conv ("NOT_A_REAL_ENCODING", "UTF-8");
    std::string out;
    CHECK_FALSE (conv.Convert ("data", out));
}

TEST_CASE ("Iconv reports failure on invalid byte sequences for the source encoding")
{
    Iconv conv ("UTF-8", "UTF-8");
    // 0xFF is never valid in UTF-8
    std::string invalid = "\xff\xfe";
    std::string out;
    CHECK_FALSE (conv.Convert (invalid, out));
}

TEST_CASE ("Iconv move constructor and move assignment transfer the conversion descriptor")
{
    Iconv original ("UTF-8", "UTF-8");
    Iconv moved (std::move (original));
    std::string out;
    CHECK (moved.Convert ("still works", out));
    CHECK (out == "still works");

    Iconv another ("ISO-8859-1", "UTF-8");
    another = std::move (moved);
    CHECK (another.Convert ("still works too", out));
    CHECK (out == "still works too");
}

TEST_CASE ("Iconv repeated conversions on the same instance reset internal state correctly")
{
    Iconv conv ("UTF-8", "UTF-8");
    std::string out;
    for (int i = 0; i < 100; i++)
    {
        std::string input = "iteration " + std::to_string (i);
        REQUIRE (conv.Convert (input, out));
        CHECK (out == input);
    }
}

TEST_CASE ("Iconv handles large input stress case")
{
    Iconv conv ("UTF-8", "UTF-8");
    std::string big (1'000'000, 'x');
    std::string out;
    REQUIRE (conv.Convert (big, out));
    CHECK (out == big);
}

/********************
 *
 * Chardet test cases
 *
 *********************/

TEST_CASE ("Chardet move construction and assignment do not crash")
{
    Chardet original;
    Chardet moved (std::move (original));
    Chardet another;
    another = std::move (moved);

    std::string encoding;
    float confidence = -1.0f;
    another.Detect ("plain ascii text", encoding, confidence);
    CHECK (confidence >= 0.0f);
}

#ifdef HAVE_CHARDET

TEST_CASE ("Chardet detects an encoding for plain ASCII text when chardet is available")
{
    Chardet det;
    std::string encoding;
    float confidence = 0.0f;
    bool ok = det.Detect ("The quick brown fox jumps over the lazy dog.", encoding, confidence);
    CHECK (ok);
    CHECK_FALSE (encoding.empty());
    CHECK (confidence >= 0.0f);
}

#else

TEST_CASE ("Chardet::Detect always fails and reports full confidence when chardet is unavailable")
{
    Chardet det;
    std::string encoding = "unchanged";
    float confidence = -1.0f;
    bool ok = det.Detect ("some data", encoding, confidence);
    CHECK_FALSE (ok);
    CHECK (confidence == 1.0f);
    CHECK (encoding == "unchanged");
}

#endif
