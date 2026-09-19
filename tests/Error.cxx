/*
    Error.cxx - contains error handling test cases
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
#include "include/Error.h"

#include <string>
#include <vector>

// A test-only error sink that just records the formatted messages it receives, so we can verify
// ErrorOutput dispatches to every registered sink without depending on global log state
class RecordingErrorSink : public ErrorSink
{
  public:
    RecordingErrorSink (std::unique_ptr<ErrorFormatter> fmt, std::vector<std::string>* out)
        : ErrorSink (std::move (fmt)), out (out)
    {}
    void Output (const Error& err) override
    {
        out->push_back (fmt->Format (err));
    }

  private:
    std::vector<std::string>* out;
};

/********************
 *
 * Error test cases
 *
 *********************/

TEST_CASE ("Error default construction has no frames and Error severity")
{
    Error err;
    CHECK (err.GetSeverity() == ErrorSeverity::Error);
    CHECK (err.GetFrameCount() == 0);
    // Accessing frames on an empty error must not crash, and returns the sentinel empty frame
    CHECK (err.RootFrame().code == ErrorCode::None);
    CHECK (err.LastFrame().code == ErrorCode::None);
    CHECK (err.GetFrames().empty());
}

TEST_CASE ("Error single-frame construction records domain, code and message")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Normal, ErrorSeverity::Warning},
        "bad thing happened");
    CHECK (err.GetSeverity() == ErrorSeverity::Warning);
    CHECK (err.GetFrameCount() == 1);
    CHECK (err.RootFrame().domain == ErrorDomain::Conf);
    CHECK (err.RootFrame().code == ErrorCode::ParseError);
    CHECK (err.RootFrame().msg == "bad thing happened");
    CHECK (&err.RootFrame() == &err.LastFrame());
}

TEST_CASE ("Error variadic constructor formats message with std::format")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "value {} is out of range [{}, {}]", 42, 0, 10);
    CHECK (err.RootFrame().msg == "value 42 is out of range [0, 10]");
}

TEST_CASE ("Error falls back to raw format string on format errors")
{
    // {:d} on a std::string is not a valid format spec and should throw std::format_error internally,
    // which Error must catch and fall back to using the raw format string as the message
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "bad spec {:d}", std::string ("oops"));
    CHECK (err.RootFrame().msg == "bad spec {:d}");
}

TEST_CASE ("Error::Add appends a frame and can downgrade severity except from Fatal")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Normal, ErrorSeverity::Error}, "first");
    err.Add ({ErrorDomain::Conf, ErrorCode::LexError, ErrorLog::Normal, ErrorSeverity::Warning}, "second");
    CHECK (err.GetFrameCount() == 2);
    CHECK (err.GetSeverity() == ErrorSeverity::Warning);
    CHECK (err.LastFrame().msg == "second");
    CHECK (err.GetFrames()[0].msg == "first");

    // Now escalate to fatal, then attempt to downgrade again - it must stick at Fatal
    err.Add ({ErrorDomain::Conf, ErrorCode::Internal, ErrorLog::Normal, ErrorSeverity::Fatal}, "third");
    CHECK (err.GetSeverity() == ErrorSeverity::Fatal);
    err.Add ({ErrorDomain::Conf, ErrorCode::Internal, ErrorLog::Normal, ErrorSeverity::Warning}, "fourth");
    CHECK (err.GetSeverity() == ErrorSeverity::Fatal);
    CHECK (err.GetFrameCount() == 4);
}

TEST_CASE ("Error::Add variadic overload formats like the constructor")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "start");
    err.Add ({ErrorDomain::Conf, ErrorCode::ParseError}, "count={}", 7);
    CHECK (err.LastFrame().msg == "count=7");
}

TEST_CASE ("Error::Add(Error&&) copies only the first frame of the argument")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "base");
    Error other ({ErrorDomain::Log, ErrorCode::FileError, ErrorLog::Normal, ErrorSeverity::Fatal}, "frameA");
    other.Add ({ErrorDomain::Log, ErrorCode::FileError}, "frameB");

    err.Add (std::move (other));
    CHECK (err.GetFrameCount() == 2);
    CHECK (err.LastFrame().msg == "frameA");
    CHECK (err.GetSeverity() == ErrorSeverity::Fatal);
}

TEST_CASE ("Error::AddByCode uses the error code string table")
{
    Error err;
    err.AddByCode ({ErrorDomain::None, ErrorCode::FileError});
    CHECK (err.LastFrame().msg == "File failure");
}

TEST_CASE ("Error::AddByCode can append errno text")
{
    errno = ENOENT;
    Error err;
    err.AddByCode ({ErrorDomain::None, ErrorCode::FileError}, true);
    CHECK (err.LastFrame().msg.starts_with ("File failure: "));
    CHECK (err.LastFrame().msg.size() > std::string ("File failure: ").size());
}

TEST_CASE ("Error::Chain wraps the current error as a cause of a new error")
{
    Error root ({ErrorDomain::Conf, ErrorCode::LexError}, "lex failed");
    Error wrapped = root.Chain ({ErrorDomain::Conf, ErrorCode::ParseError}, "parse failed");

    CHECK (wrapped.RootFrame().msg == "parse failed");
    CHECK (wrapped.Cause().RootFrame().msg == "lex failed");
}

TEST_CASE ("Error::Chain variadic overload formats the new frame")
{
    Error root ({ErrorDomain::Conf, ErrorCode::LexError}, "lex failed");
    Error wrapped = root.Chain ({ErrorDomain::Conf, ErrorCode::ParseError}, "parse failed at line {}", 12);
    CHECK (wrapped.RootFrame().msg == "parse failed at line 12");
}

TEST_CASE ("Error::GetChain flattens a causal chain in root-to-leaf order")
{
    Error a ({ErrorDomain::Conf, ErrorCode::LexError}, "a");
    Error b = a.Chain ({ErrorDomain::Conf, ErrorCode::ParseError}, "b");
    Error c = b.Chain ({ErrorDomain::Log, ErrorCode::Internal}, "c");

    auto chain = Error::GetChain (c);
    REQUIRE (chain.size() == 3);
    CHECK (chain[0].RootFrame().msg == "a");
    CHECK (chain[1].RootFrame().msg == "b");
    CHECK (chain[2].RootFrame().msg == "c");
}

TEST_CASE ("Error::GetChain on an error with no cause is a single-element chain")
{
    Error a ({ErrorDomain::Conf, ErrorCode::LexError}, "solo");
    auto chain = Error::GetChain (a);
    REQUIRE (chain.size() == 1);
    CHECK (chain[0].RootFrame().msg == "solo");
}

TEST_CASE ("Error copy constructor deep-copies the cause chain")
{
    Error a ({ErrorDomain::Conf, ErrorCode::LexError}, "a");
    Error b = a.Chain ({ErrorDomain::Conf, ErrorCode::ParseError}, "b");

    Error copy (b);
    CHECK (copy.RootFrame().msg == "b");
    CHECK (copy.Cause().RootFrame().msg == "a");

    // Mutating the original's cause chain must not affect the copy
    b.Add ({ErrorDomain::Conf, ErrorCode::ParseError}, "b-extra");
    CHECK (copy.GetFrameCount() == 1);
}

TEST_CASE ("Error copy-assignment deep-copies the cause chain")
{
    Error a ({ErrorDomain::Conf, ErrorCode::LexError}, "a");
    Error b = a.Chain ({ErrorDomain::Conf, ErrorCode::ParseError}, "b");

    Error copy;
    copy = b;
    CHECK (copy.RootFrame().msg == "b");
    CHECK (copy.Cause().RootFrame().msg == "a");

    // Self-assignment must be a no-op and not crash or corrupt state
    copy = copy;
    CHECK (copy.RootFrame().msg == "b");
}

TEST_CASE ("Error move constructor and move assignment transfer state")
{
    Error a ({ErrorDomain::Conf, ErrorCode::LexError}, "moveme");
    Error moved (std::move (a));
    CHECK (moved.RootFrame().msg == "moveme");

    Error target;
    target = std::move (moved);
    CHECK (target.RootFrame().msg == "moveme");
}

TEST_CASE ("ErrorException exposes the root frame message and the wrapped error")
{
    Error err ({ErrorDomain::Log, ErrorCode::FileError}, "disk on fire");
    ErrorException ex (err);
    CHECK (std::string (ex.what()) == "disk on fire");
    CHECK (ex.Error().RootFrame().msg == "disk on fire");
}

TEST_CASE ("ResCustom stores a value on the success path")
{
    Result<int> res (42);
    REQUIRE (res);
    CHECK (res.Value() == 42);
}

TEST_CASE ("ResCustom stores an error on the failure path")
{
    Result<int> res (Error ({ErrorDomain::Conf, ErrorCode::ParseError}, "nope"));
    REQUIRE_FALSE (res);
    CHECK (res.Error().RootFrame().msg == "nope");
}

TEST_CASE ("ResNone / Success represents a valueless successful result")
{
    ResNone res = Success();
    CHECK (res);
}

TEST_CASE ("UserErrorFormatter prints the last frame first followed by normal-log frames in reverse")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Normal}, "root cause");
    err.Add ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Debug}, "debug detail");
    err.Add ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Normal}, "final message");

    UserErrorFormatter fmt (false);
    std::string out = fmt.Format (err);
    // Non-verbose must skip the debug-only frame
    CHECK (out == "final message: root cause");
}

TEST_CASE ("UserErrorFormatter includes debug frames when verbose is enabled")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Normal}, "root cause");
    err.Add ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Debug}, "debug detail");
    err.Add ({ErrorDomain::Conf, ErrorCode::ParseError, ErrorLog::Normal}, "final message");

    UserErrorFormatter fmt (true);
    std::string out = fmt.Format (err);
    CHECK (out == "final message: debug detail: root cause");
}

TEST_CASE ("UserErrorFormatter on a single-frame error just prints that frame")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "only one");
    UserErrorFormatter fmt;
    CHECK (fmt.Format (err) == "only one");
}

TEST_CASE ("TraceErrorFormatter prints every frame in order with arrow markers")
{
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "first");
    err.Add ({ErrorDomain::Conf, ErrorCode::ParseError}, "second");
    err.Add ({ErrorDomain::Conf, ErrorCode::ParseError}, "third");

    TraceErrorFormatter fmt;
    std::string out = fmt.Format (err);
    CHECK (out == "third\n    -> second\n    -> first");
}

TEST_CASE ("ErrorOutput dispatches a report to every registered sink")
{
    // ErrorOutput is a process-wide singleton with no way to unregister a sink, so the vectors these
    // sinks write into must outlive this test case (other tests/lexer warnings may report errors
    // through the singleton for the rest of the process's lifetime) - use static storage rather than
    // stack locals to avoid leaving dangling pointers registered.
    static std::vector<std::string> sinkA;
    static std::vector<std::string> sinkB;
    ErrorOutput::The()->AddSink (std::make_unique<RecordingErrorSink> (std::make_unique<UserErrorFormatter>(), &sinkA));
    ErrorOutput::The()->AddSink (
        std::make_unique<RecordingErrorSink> (std::make_unique<TraceErrorFormatter>(), &sinkB));

    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "broadcast me");
    ErrorOutput::The()->Report (err);

    REQUIRE (sinkA.size() == 1);
    REQUIRE (sinkB.size() == 1);
    CHECK (sinkA[0] == "broadcast me");
    CHECK (sinkB[0] == "broadcast me");
}

TEST_CASE ("ErrorOutput::The always returns the same singleton instance")
{
    CHECK (ErrorOutput::The() == ErrorOutput::The());
}

TEST_CASE ("Error stress test with a large number of frames and a deep causal chain")
{
    constexpr int frameCount = 5000;
    Error err ({ErrorDomain::Conf, ErrorCode::ParseError}, "frame-0");
    for (int i = 1; i < frameCount; i++)
        err.Add ({ErrorDomain::Conf, ErrorCode::ParseError}, "frame-{}", i);

    CHECK (err.GetFrameCount() == static_cast<size_t> (frameCount));
    CHECK (err.RootFrame().msg == "frame-0");
    CHECK (err.LastFrame().msg == "frame-4999");

    // Build a long causal chain out of small errors and make sure GetChain handles it
    constexpr int chainDepth = 500;
    Error chainErr ({ErrorDomain::Conf, ErrorCode::ParseError}, "chain-0");
    for (int i = 1; i < chainDepth; i++)
        chainErr = chainErr.Chain ({ErrorDomain::Conf, ErrorCode::ParseError}, "chain-{}", i);

    auto chain = Error::GetChain (chainErr);
    REQUIRE (chain.size() == static_cast<size_t> (chainDepth));
    for (int i = 0; i < chainDepth; i++)
        CHECK (chain[i].RootFrame().msg == "chain-" + std::to_string (i));
}
