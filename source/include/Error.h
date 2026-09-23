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
#include "include/StringHash.h"

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
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/core.h>
#include <fmt/args.h>

enum class ErrorDomain
{
    None,
    Log,
    Conf,
    Action,
    Task,
    Option,
    Operation,
    Image,
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

using ErrorProp = std::pair<std::string, std::string>;
using ErrorKeyMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;

struct ErrorFrame
{
    ErrorFrame (ErrorDomain domain, ErrorCode code, std::string_view msg, ErrorLog log, ErrorKeyMap props = {})
        : domain (domain), code (code), log (log), msg (msg), timestamp (std::chrono::system_clock::now()), keys (props)
    {}
    ErrorDomain domain;
    ErrorCode code;
    ErrorLog log;
    std::string msg;
    std::chrono::system_clock::time_point timestamp;
    ErrorKeyMap keys;
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
        : severity (other.severity), frames (other.frames), context (other.context),
          cause (other.cause ? std::make_unique<Error> (*other.cause) : nullptr)
    {}

    Error (const ErrorInfo& info, std::initializer_list<ErrorProp> props) : severity (info.severity)
    {
        ErrorKeyMap map{props.begin(), props.end()};
        // Make the message
        std::string msg = makeMessage (info.code, map);
        frames.emplace_back (info.domain, info.code, msg, info.log, std::move (map));
    }

    // Property-based interface with a caller-supplied message, skipping the default lookup-table formatting
    Error (const ErrorInfo& info, std::string_view msg, std::initializer_list<ErrorProp> props)
        : severity (info.severity)
    {
        ErrorKeyMap map{props.begin(), props.end()};
        frames.emplace_back (info.domain, info.code, msg, info.log, std::move (map));
    }

    template <typename... Args>
        requires (sizeof...(Args) > 0)
    Error (const ErrorInfo& info, std::string_view fmt, std::initializer_list<ErrorProp> props, const Args&... args)
        : Error (info, formatMessage (fmt, args...), props)
    {}

    Error& operator= (const Error& other)
    {
        if (this != &other)
        {
            severity = other.severity;
            frames = other.frames;
            context = other.context;
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
    // Property-based interface with a caller-supplied message, skipping the default lookup-table formatting
    virtual Error& Add (const ErrorInfo& info, std::string_view msg, std::initializer_list<ErrorProp> props)
    {
        if (this->severity != ErrorSeverity::Fatal)
            this->severity = info.severity;
        ErrorKeyMap map{props.begin(), props.end()};
        frames.emplace_back (info.domain, info.code, msg, info.log, std::move (map));
        return *this;
    }

    template <typename... Args>
        requires (sizeof...(Args) > 0)
    Error& Add (const ErrorInfo& info,
        std::string_view fmt,
        std::initializer_list<ErrorProp> props,
        const Args&... args)
    {
        return Add (info, formatMessage (fmt, args...), props);
    }

    Error& Add (const ErrorInfo& info, std::initializer_list<ErrorProp> props)
    {
        if (this->severity != ErrorSeverity::Fatal)
            this->severity = info.severity;

        ErrorKeyMap map{props.begin(), props.end()};
        std::string msg = makeMessage (info.code, map);
        frames.emplace_back (info.domain, info.code, msg, info.log, std::move (map));
        return *this;
    }

    Error& AddContext (std::initializer_list<ErrorProp> props)
    {
        for (const auto& [key, value] : props)
            context[key] = value;
        if (cause)
            cause->AddContext (props);
        return *this;
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

    // Helper to add an error based off errno
    Error& AddByErrno (const ErrorInfo& info)
    {
        return Add (info, std::strerror (errno));
    }

    // These functions are for error chaining. This is where one error creates a whole new error that is
    // seperate from the original
    virtual Error Chain (const ErrorInfo& info, std::string_view msg)
    {
        Error err = Error (info, msg);
        err.context = context;
        err.cause = std::make_unique<Error> (std::move (*this));
        return err;
    }

    template <typename... Args>
        requires (sizeof...(Args) > 0)
    Error Chain (const ErrorInfo& info, std::string_view fmt, const Args&... args)
    {
        return Chain (info, formatMessage (fmt, args...));
    }

    // Property-based interface with a caller-supplied message, skipping the default lookup-table formatting
    virtual Error Chain (const ErrorInfo& info, std::string_view msg, std::initializer_list<ErrorProp> props)
    {
        Error err = Error (info, msg, props);
        err.context = context;
        err.cause = std::make_unique<Error> (std::move (*this));
        return err;
    }

    template <typename... Args>
        requires (sizeof...(Args) > 0)
    Error Chain (const ErrorInfo& info,
        std::string_view fmt,
        std::initializer_list<ErrorProp> props,
        const Args&... args)
    {
        return Chain (info, formatMessage (fmt, args...), props);
    }

    // Table-driven chaining
    Error Chain (const ErrorInfo& info, std::initializer_list<ErrorProp> props)
    {
        Error err = Error (info, props);
        err.context = context;
        err.cause = std::make_unique<Error> (std::move (*this));
        return err;
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
    const ErrorKeyMap& GetContext() const
    {
        return context;
    }

    // TODO: this function needs to be re-thought out
    std::string MakeContextStr() const
    {
        std::string result;
        auto file = context.find ("file");
        auto line = context.find ("line");
        if (file != context.end())
        {
            result = file->second;
            if (line != context.end())
                result += ":" + line->second;
        }
        else if (line != context.end())
        {
            result = line->second;
        }

        for (const auto& [key, value] : context)
        {
            if (key == "file" || key == "line")
                continue;
            if (!result.empty())
                result += " ";
            result += std::format ("{}:{}", key, value);
        }
        if (!result.empty())
            result += ": ";
        return result;
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
    ErrorKeyMap context;
    std::unique_ptr<Error> cause = nullptr;    // For casual chaining, so one error has multiple messages

  private:
    template <typename... Args>
    static std::string formatMessage (std::string_view fmt, const Args&... args)
    {
        if (fmt.empty())
            return {};
        try
        {
            return fmt::vformat (fmt, fmt::make_format_args (args...));
        }
        catch (const fmt::format_error&)
        {
            return std::string (fmt);
        }
    }

    static std::string formatMessage (std::string_view fmt,
        const fmt::dynamic_format_arg_store<fmt::format_context>& store)
    {
        if (fmt.empty())
            return {};
        try
        {
            return fmt::vformat (fmt, store);
        }
        catch (const fmt::format_error&)
        {
            return std::string (fmt);
        }
    }

    std::string makeMessage (ErrorCode code, const ErrorKeyMap& map)
    {
        const ErrorEntry& entry = _errorCodeStrings[code];
        fmt::dynamic_format_arg_store<fmt::format_context> store;
        for (std::string_view param : entry.params)
        {
            auto it = map.find (param);
            if (it == map.end())
                throw std::runtime_error ("Required error paramter not passed");

            store.push_back (it->second);
        }
        return formatMessage (entry.str, store);
    }

    // Used when there are no frames to report
    inline static const ErrorFrame emptyFrame{ErrorDomain::None, ErrorCode::None, "", ErrorLog::Normal};
};

// NOTE: This is to be used sparingly. Exceptions are only meant for programming errors and not
// anything else. THe ONLY exception to this rule is in constructors when a factory function is
// impractial
// Just try to catch the exception as close to the source as possible
// NOTE2: As a rule of thumb, if an error could happen during normal execution, return as a result
// If it probably couldn't occur but is still a good safety net (e.g. improper function usage) throw
// For complete invariants, assert
class ErrorException : public std::exception
{
  public:
    explicit ErrorException (const ::Error& error) : error (error)
    {}
    const char* what() const noexcept override
    {
        return error.RootFrame().msg.c_str();
    }
    ::Error& Error() noexcept
    {
        return error;
    }
    const ::Error& Error() const noexcept
    {
        return error;
    }

  private:
    class Error error;
};

// Rudimentary Result class
template <typename T, class E>
class ResCustom
{
  public:
    ResCustom (T val) : error (std::nullopt), ok (true), value (std::move (val))
    {
        static_assert (std::is_base_of_v<::Error, E>, "ResCustom<E> type must inherit from Error");
        static_assert (!std::is_same_v<T, E>, "ResCustom<T,E> can't have same type");
    }
    ResCustom (E error) : error (std::move (error)), ok (false), value (std::nullopt)
    {
        static_assert (std::is_base_of_v<::Error, E>, "ResCustom<E> type must inherit from Error");
        static_assert (!std::is_same_v<T, E>, "ResCustom<T,E> can't have same type");
    }
    bool Ok() const
    {
        return ok;
    }
    explicit operator bool() const
    {
        return ok;
    }
    T& Value()
    {
        assert (value.has_value());
        return *value;
    }
    const T& Value() const
    {
        assert (value.has_value());
        return *value;
    }
    E& Error()
    {
        assert (error.has_value());
        return *error;
    }
    const E& Error() const
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
                e = e.Add ({ErrorDomain::None, ErrorCode::ErrorReportMissing}, {});

            e = e.Add ({ErrorDomain::None, ErrorCode::ErrorReportOpen}, {{"file", file}});

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
