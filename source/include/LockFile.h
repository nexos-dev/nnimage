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

#include <string>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>

// TODO: we should RAII this
class LockFile
{
  public:
    LockFile (const std::string& path)
    {
        this->path = path;
        fd = open (path.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd == -1)
        {
            throw ErrorException (
                Error ({ErrorDomain::Log, ErrorCode::FileError},
                       "Failed to open lock file: " + path)
                    .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true));
        }
    }
    // Ensures we don't leave a ghost lock file behind
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
        setFileLock (fd, F_SETLK, F_UNLCK);
        locked = false;
    }
    Result<bool> ReadLock (bool block = false)
    {
        setFileLock (fd, block ? F_SETLKW : F_SETLK, F_RDLCK);
        if (errno == EACCES || errno == EAGAIN)
            return Result<bool> (false);
        else if (errno != 0)
        {
            return Result<bool> (
                Error ({ErrorDomain::Log, ErrorCode::FileError},
                       "Failed to acquire read lock on lock file: " + path)
                    .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true));
        }
        locked = true;
        return true;
    }
    Result<bool> WriteLock (bool block = false)
    {
        setFileLock (fd, block ? F_SETLKW : F_SETLK, F_WRLCK);
        if (errno == EACCES || errno == EAGAIN)
            return Result<bool> (false);
        else if (errno != 0)
        {
            return Result<bool> (
                Error ({ErrorDomain::Log, ErrorCode::FileError},
                       "Failed to acquire write lock on lock file: " + path)
                    .AddByCode ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Debug}, true));
        }
        locked = true;
        return true;
    }

  private:
    int setFileLock (int fd, int cmd, int type)
    {
        struct flock fl;
        fl.l_type = type;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;    // Lock the whole file
        return fcntl (fd, cmd, &fl);
    }
    bool locked = false;
    std::string path;
    int fd = -1;
};

#endif
