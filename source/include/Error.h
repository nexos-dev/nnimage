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

#include "ErrorCode.h"

enum class ErrorDomain
{
    None,
    Log,
    Conf,
    Action,
    Task,
    Option,
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
    ErrorFrame (ErrorDomain domain, ErrorCode code, const std::string& msg, ErrorLog log = ErrorLog::Normal)
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

// NOTE: I hear people saying, why are you using printf formatting?
// Well, because I am old school and prefer it. Bad reasoning? Yes. Do I care? no.
class Error
{
  public:
    Error() : severity (ErrorSeverity::Error)
    {}
    Error (const ErrorInfo& info, const std::string& msg)
    {
        ErrorFrame frame (info.domain, info.code, std::move (msg), info.log);
        this->severity = info.severity;
        frames.push_back (frame);
    }
    template <typename... Args>
    Error (const ErrorInfo& info, const std::string& fmt, const Args&... args)
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

    Error& Add (const ErrorInfo& info, const std::string& msg)
    {
        ErrorFrame frame (info.domain, info.code, std::move (msg), info.log);
        // Never downgrade from fatal, but we can downgrade from error to warning
        if (this->severity != ErrorSeverity::Fatal)
            this->severity = info.severity;
        frames.push_back (frame);
        return *this;
    }
    template <typename... Args>
    Error& Add (const ErrorInfo& info, const std::string& fmt, const Args&... args)
    {
        return Add (info, formatMessage (fmt, args...));
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
    Error Chain (const ErrorInfo& info, const std::string& msg)
    {
        Error err = Error (info, msg);
        err.cause = std::make_unique<Error> (std::move (*this));
        return err;
    }
    template <typename... Args>
    Error Chain (const ErrorInfo& info, const std::string& fmt, const Args&... args)
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

  private:
    // Wrappers so std::string in a va_args list is implicitly converted to a C string
    // I know I know, I should just use std::format but I really don't want to
    template <typename T>
    static const T& treatString (const T& arg)
    {
        return arg;
    }
    inline static const char* treatString (const std::string& arg)
    {
        return arg.c_str();
    }
    template <typename... Args>
    static std::string formatMessage (const std::string& fmt, const Args&... args)
    {
        if (fmt.empty())
            return {};
        int size = std::snprintf (nullptr, 0, fmt.c_str(), treatString (args)...);
        if (size < 0)
            return std::string (fmt);
        std::string msg (size + 1, 0);
        std::snprintf (msg.data(), msg.size(), fmt.c_str(), treatString (args)...);
        msg.resize (size);
        return msg;
    }
    // Severity is not frame specific, this is because we want the whole error to be treated as a
    // single severity. For example, if say one image fails to write, but there are still other
    // images, its not fatal. If it's the only one then from the user's perspective, it is fatal
    ErrorSeverity severity;
    std::vector<ErrorFrame> frames;
    std::unique_ptr<Error> cause;    // For casual chaining, so one error has multiple messages
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

// Rudimentary Result class. Gives Result<T,E> without the E
template <typename T>
class Result
{
  public:
    Result (T val) : error (std::nullopt), ok (true), value (std::move (val))
    {}
    Result (Error error) : error (std::move (error)), ok (false), value (std::nullopt)
    {}
    bool IsOk() const
    {
        return ok;
    }
    T& GetValue()
    {
        if (!value.has_value())
            throw std::runtime_error ("Result does not contain a value");
        return *value;
    }
    Error& GetError()
    {
        if (!error.has_value())
            throw std::runtime_error ("Result is in an invalid state");
        return *error;
    }
    const Error& GetError() const
    {
        if (!error.has_value())
            throw std::runtime_error ("Result is in an invalid state");
        return *error;
    }

  private:
    std::optional<T> value;
    bool ok = true;
    std::optional<Error> error;
};

// A little helper to make the fact that a result object contains no result clearer
using ResNone = Result<std::monostate>;
using Success = std::monostate;

// Base class for formatting errors for output
class ErrorFormatter
{
  public:
    virtual ~ErrorFormatter() = default;
    virtual std::string Format (const Error& err) = 0;
};

// NOTE: this is and must be a singleton
class ErrorOutput
{
  public:
    void AddFormatter (std::unique_ptr<ErrorFormatter> fmt)
    {
        formats.push_back (std::move (fmt));
    }
    void Report (const Error& err)
    {
        for (const auto& fmt : formats)
            fmt->Format (err);
    }

  private:
    std::vector<std::unique_ptr<ErrorFormatter>> formats;
};

extern std::unique_ptr<ErrorOutput> _errOut;

#endif
