/*
    LockFile.h - A simple class to create and manage a lock file. Used in a number of areas
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

#ifndef LOCKFILE_H
#define LOCKFILE_H

#include "include/Error.h"

#include <unistd.h>
#include <fcntl.h>

#include <filesystem>
#include <optional>
#include <string>

class LockFile
{
  public:
    LockFile() = default;
    LockFile (std::string path)
    {
        this->path = std::move (path);
        fd = open (this->path.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd == -1)
        {
            throw ErrorException (
                Error ({ErrorDomain::None, ErrorCode::FileError}, "Failed to open lock file: " + this->path)
                    .AddByCode ({ErrorDomain::None, ErrorCode::FileError, ErrorLog::Debug}, true));
        }
    }
    LockFile (const std::filesystem::path& path) : LockFile (path.string())
    {}
    ~LockFile()
    {
        close (fd);
        fd = -1;
    }

    bool IsLocked()
    {
        return locked;
    }
    void Unlock()
    {
        if (locked)
        {
            assert (fd != -1);
            setFileLock (fd, F_SETLK, F_UNLCK);
            locked = false;
        }
    }
    bool ReadLock (bool block = false)
    {
        assert (fd != -1);
        bool res = setFileLock (fd, block ? F_SETLKW : F_SETLK, F_RDLCK);
        if (res)
            locked = true;
        return res;
    }
    bool WriteLock (bool block = false)
    {
        assert (fd != -1);
        bool res = setFileLock (fd, block ? F_SETLKW : F_SETLK, F_WRLCK);
        if (res)
            locked = true;
        return res;
    }

    LockFile (const LockFile& other) = delete;
    LockFile& operator= (const LockFile& other) = delete;

    LockFile (LockFile&& other) : fd{other.fd}, path{std::move (other.path)}, locked{other.locked}
    {
        other.fd = -1;
        other.locked = false;
    }
    LockFile& operator= (LockFile&& other)
    {
        if (this == &other)
            return *this;
        if (fd != -1)
            close (fd);
        fd = other.fd;
        path = std::move (other.path);
        locked = other.locked;
        other.fd = -1;
        other.locked = false;
        return *this;
    }

  private:
    bool setFileLock (int fd, int cmd, int type)
    {
        struct flock fl{};
        fl.l_type = type;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;    // Lock the whole file
        if (fcntl (fd, cmd, &fl) < 0)
        {
            if (errno == EACCES || errno == EAGAIN)
                return false;
            throw ErrorException (
                Error ({ErrorDomain::Log, ErrorCode::FileError}, "Failed to acquire lock on file \"{}\": ", path)
                    .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true));
        }
        return true;
    }
    bool locked = false;
    std::string path;
    int fd = -1;
};

class LockFileShared
{
  public:
    LockFileShared() = default;
    LockFileShared (LockFile&& lock) : lock (std::move (lock))
    {
        lock.ReadLock (true);
    }
    LockFileShared (LockFileShared&& other) noexcept : lock (std::move (other.lock))
    {}
    LockFileShared& operator= (LockFileShared&& other) noexcept
    {
        if (this == &other)
            return *this;
        lock = std::move (other.lock);
        return *this;
    }
    ~LockFileShared()
    {
        lock.Unlock();
    }
    static std::optional<LockFileShared> TryAcquire (std::string path)
    {
        LockFileShared lock;
        lock.lock = LockFile (std::move (path));
        bool res = lock.lock.ReadLock();
        if (!res)
            return {};
        return std::move (lock);
    }
    static std::optional<LockFileShared> TryAcquire (const std::filesystem::path& path)
    {
        return TryAcquire (path.string());
    }

  private:
    LockFile lock;
};

class LockFileUnique
{
  public:
    LockFileUnique() = default;
    LockFileUnique (LockFile&& lock) : lock (std::move (lock))
    {
        lock.WriteLock (true);
    }
    LockFileUnique (LockFileUnique&& other) noexcept : lock (std::move (other.lock))
    {}
    LockFileUnique& operator= (LockFileUnique&& other) noexcept
    {
        if (this == &other)
            return *this;
        lock = std::move (other.lock);
        return *this;
    }
    ~LockFileUnique()
    {
        lock.Unlock();
    }
    static std::optional<LockFileUnique> TryAcquire (std::string path)
    {
        LockFileUnique lock;
        lock.lock = LockFile (std::move (path));
        bool res = lock.lock.WriteLock();
        if (!res)
            return {};
        return std::move (lock);
    }
    static std::optional<LockFileUnique> TryAcquire (const std::filesystem::path& path)
    {
        return TryAcquire (path.string());
    }

  private:
    LockFile lock;
};

#endif
