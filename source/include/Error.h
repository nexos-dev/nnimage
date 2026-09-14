/*
    Error.h - contains error handling classes and codes
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

#ifndef ERROR_H
#define ERROR_H

#include "sys/ErrorCode.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

enum class ErrorDomain
{
    None,
    Log,
    Conf,
    Action,
    Task,
    Option,
    Operation,
    ImageConf,
    Backend
};

enum class ErrorSeverity
{
    Warning,
    Error,
    Fatal
};

enum class ErrorLog
{
    Normal,
    Debug
};

struct ErrorFrame
{
    ErrorFrame (ErrorDomain domain, ErrorCode code, std::string_view msg, ErrorLog log = ErrorLog::Normal)
        : domain (domain), code (code), log (log), msg (msg), timestamp (std::chrono::system_clock::now())
    {}
    ErrorDomain domain;
    ErrorCode code;
    ErrorLog log;
    std::string msg;
    std::chrono::system_clock::time_point timestamp;
};

struct ErrorInfo
{
    ErrorDomain domain;
    ErrorCode code;
    ErrorLog log = ErrorLog::Normal;
    ErrorSeverity severity = ErrorSeverity::Error;
};

class Error
{
  public:
    Error() : severity (ErrorSeverity::Error)
    {}
    Error (const ErrorInfo& info, std::string_view msg) : severity (info.severity)
    {
        frames.emplace_back (info.domain, info.code, msg, info.log);
    }
    template <typename... Args>
        requires (sizeof...(Args) > 0)
    Error (const ErrorInfo& info, std::string_view fmt, const Args&... args)
        : Error (info, formatMessage (fmt, args...))
    {}
    Error (const Error& other)
        : severity (other.severity), frames (other.frames),
          cause (other.cause ? std::make_unique<Error> (*other.cause) : nullptr)
    {}
    Error& operator= (const Error& other)
    {
        if (this != &other)
        {
            severity = other.severity;
            frames = other.frames;
            cause = other.cause ? std::make_unique<Error> (*other.cause) : nullptr;
        }
        return *this;
    }
    Error (Error&&) = default;
    Error& operator= (Error&&) = default;
    virtual ~Error() = default;

    virtual Error& Add (const ErrorInfo& info, std::string_view msg)
    {
        // Never downgrade from fatal, but we can downgrade from error to warning
        if (this->severity != ErrorSeverity::Fatal)
            this->severity = info.severity;
        frames.emplace_back (info.domain, info.code, msg, info.log);
        return *this;
    }
    template <typename... Args>
        requires (sizeof...(Args) > 0)
    Error& Add (const ErrorInfo& info, std::string_view fmt, const Args&... args)
    {
        return Add (info, formatMessage (fmt, args...));
    }
    // Used mostly so we can pass on overrided class to Add. Parameter must be rvalue
    // TODO: maybe we should allow lvalues?
    Error& Add (const Error&& err)
    {
        // Never downgrade from fatal, matching the semantics of the other Add() overloads
        if (this->severity != ErrorSeverity::Fatal)
            this->severity = err.severity;
        frames.push_back (err.frames[0]);
        return *this;
    }

    // Helper to add an error based strictly off of a code
    Error& AddByCode (const ErrorInfo& info, bool includeErrno = false)
    {
        std::string msg = _errorCodeStrings[info.code];
        if (includeErrno)
            msg += std::string (": ") + std::strerror (errno);
        return Add (info, msg);
    }

    // These functions are for error chaining. This is where one error creates a whole new error that is
    // seperate from the original
    virtual Error Chain (const ErrorInfo& info, std::string_view msg)
    {
        Error err = Error (info, msg);
        err.cause = std::make_unique<Error> (std::move (*this));
        return err;
    }
    template <typename... Args>
        requires (sizeof...(Args) > 0)
    Error Chain (const ErrorInfo& info, std::string_view fmt, const Args&... args)
    {
        return Chain (info, formatMessage (fmt, args...));
    }

    ErrorSeverity GetSeverity() const
    {
        return severity;
    }
    const ErrorFrame& RootFrame() const
    {
        if (frames.empty())
            return emptyFrame;
        return frames[0];
    }
    const ErrorFrame& LastFrame() const
    {
        if (frames.empty())
            return emptyFrame;
        return frames[frames.size() - 1];
    }
    const std::vector<ErrorFrame>& GetFrames() const
    {
        return frames;
    }
    size_t GetFrameCount() const
    {
        return frames.size();
    }
    const Error& Cause()
    {
        return *cause;
    }

    // Takes the casual chain, and turns it into an array
    static std::deque<Error> GetChain (const Error& err)
    {
        std::deque<Error> chain;
        chain.push_front (err);
        Error* cur = err.cause.get();
        while (cur)
        {
            chain.push_front (*cur);
            cur = cur->cause.get();
        }
        return chain;
    }

  protected:
    // Severity is not frame specific, this is because we want the whole error to be treated as a
    // single severity. For example, if say one image fails to write, but there are still other
    // images, its not fatal. If it's the only one then from the user's perspective, it is fatal
    ErrorSeverity severity;
    std::vector<ErrorFrame> frames;
    std::unique_ptr<Error> cause = nullptr;    // For casual chaining, so one error has multiple messages
  private:
    template <typename... Args>
    static std::string formatMessage (std::string_view fmt, const Args&... args)
    {
        if (fmt.empty())
            return {};
        try
        {
            return std::vformat (fmt, std::make_format_args (args...));
        }
        catch (const std::format_error&)
        {
            return std::string (fmt);
        }
    }
    // Used when there are no frames to report
    inline static const ErrorFrame emptyFrame{ErrorDomain::None, ErrorCode::None, ""};
};

// NOTE: This is to be used sparingly. Exceptions are only meant for programming errors and not
// anything else. THe ONLY exception to this rule is in constructors when a factory function is
// impractial
// Just try to catch the exception as close to the source as possible
// NOTE2: generally for programming errors, assert is preferred
class ErrorException : public std::exception
{
  public:
    explicit ErrorException (const Error& error) : error (error)
    {}
    const char* what() const noexcept override
    {
        return error.RootFrame().msg.c_str();
    }
    const Error& GetError() const noexcept
    {
        return error;
    }

  private:
    Error error;
};

// Rudimentary Result class
template <typename T, class E>
class ResCustom
{
  public:
    ResCustom (T val) : error (std::nullopt), ok (true), value (std::move (val))
    {}
    ResCustom (E error) : error (std::move (error)), ok (false), value (std::nullopt)
    {
        static_assert (std::is_base_of_v<Error, E>, "Result<E> type must inherit from Error");
    }
    bool IsOk() const
    {
        return ok;
    }
    T& GetValue()
    {
        assert (value.has_value());
        return *value;
    }
    E& GetError()
    {
        assert (error.has_value());
        return *error;
    }
    const E& GetError() const
    {
        assert (error.has_value());
        return *error;
    }

  private:
    std::optional<T> value;
    bool ok = true;
    std::optional<E> error;
};

template <typename T>
using Result = ResCustom<T, Error>;

// A little helper to make the fact that a result object contains no result clearer
using NoResult = std::monostate;
using ResNone = Result<std::monostate>;
using Success = std::monostate;

class ErrorFormatter
{
  public:
    virtual ~ErrorFormatter() = default;
    virtual std::string Format (const Error& err) = 0;
};

class ErrorSink
{
  public:
    ErrorSink (std::unique_ptr<ErrorFormatter> fmt) : fmt{std::move (fmt)}
    {}
    virtual ~ErrorSink() = default;
    virtual void Output (const Error& err) = 0;

  protected:
    std::unique_ptr<ErrorFormatter> fmt;
};

// NOTE: this is and must be a singleton
class ErrorOutput
{
  public:
    void AddSink (std::unique_ptr<ErrorSink> sink)
    {
        sinks.push_back (std::move (sink));
    }
    void Report (const Error& err)
    {
        for (auto& sink : sinks)
            sink->Output (err);
    }

    static ErrorOutput* The()
    {
        static ErrorOutput the;
        return &the;
    }

  private:
    ErrorOutput() = default;
    std::vector<std::unique_ptr<ErrorSink>> sinks;
};

class LogErrorSink : public ErrorSink
{
  public:
    LogErrorSink (std::unique_ptr<ErrorFormatter> fmt) : ErrorSink{std::move (fmt)}
    {}
    void Output (const Error& err);
};

class FileErrorSink : public ErrorSink
{
  public:
    FileErrorSink (const std::string& file, std::unique_ptr<ErrorFormatter> fmt)
        : file{file}, ErrorSink{std::move (fmt)}
    {
        if (!this->file.is_open())
        {
            Error e;
            if (!std::filesystem::exists (file))
                e = e.Add ({ErrorDomain::None, ErrorCode::FileError}, "No such file or directory");

            e = e.Add ({ErrorDomain::None, ErrorCode::FileError}, "Unable to open error reporting file \"{}\"", file);

            throw ErrorException (e);
        }
    }
    void Output (const Error& err);

  private:
    std::ofstream file;
    std::mutex lock;
};

class UserErrorFormatter : public ErrorFormatter
{
  public:
    UserErrorFormatter (bool verbose = false) : verbose{verbose}
    {}
    std::string Format (const Error& err);

  private:
    bool verbose = false;
};

class TraceErrorFormatter : public ErrorFormatter
{
  public:
    std::string Format (const Error& err);
};

#endif
