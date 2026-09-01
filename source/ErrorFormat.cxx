/*
    ErrorFormat.cxx - contains error formatters
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
#include "include/Timestamp.h"

void FileErrorSink::Output (const Error& err)
{
    auto errors = Error::GetChain (err);
    for (const auto& err : errors)
    {
        std::lock_guard<std::mutex> guard (lock);
        file << fmt->Format (err) << "\n";
    }
    file.flush();
}

void LogErrorSink::Output (const Error& err)
{
    auto errors = Error::GetChain (err);
    for (const auto& err : errors)
    {
        std::string errorMsg = fmt->Format (err);
        switch (err.GetSeverity())
        {
            case ErrorSeverity::Warning:
                _log->Warning (errorMsg);
                break;
            case ErrorSeverity::Error:
                _log->Error (errorMsg);
                break;
            case ErrorSeverity::Fatal:
                _log->Fatal (errorMsg);
                break;
        }
    }
}

std::string UserErrorFormatter::Format (const Error& err)
{
    // Start with last message
    std::stringstream out;
    const ErrorFrame& lastFrame = err.LastFrame();
    out << lastFrame.msg;

    // Now add the others
    const auto& frames = err.GetFrames();
    int i = frames.size() - 2;
    for (; i >= 0; i--)
    {
        const ErrorFrame& frame = frames[i];
        if (frame.log == ErrorLog::Normal || verbose)
            out << ": " << frame.msg;
    }
    return out.str();
}

std::string TraceErrorFormatter::Format (const Error& err)
{
    // Start with last message
    std::stringstream out;
    const ErrorFrame& lastFrame = err.LastFrame();
    out << lastFrame.msg;

    // Now add the others
    const auto& frames = err.GetFrames();
    int i = frames.size() - 2;
    for (; i >= 0; i--)
    {
        const ErrorFrame& frame = frames[i];
        out << "\n    -> " << frame.msg;
    }
    return out.str();
}
