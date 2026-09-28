/*
    Dispatch.cxx - contains main dispatcher layer
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

#include "include/Dispatch.h"
#include "include/Operation.h"
#include "include/Error.h"
#include "include/Log.h"
#include "include/OpTable.h"

#include <memory>

Result<std::filesystem::path> Dispatch::getLogDir()
{
    auto dir = getConfigDir() / "log";
    // Ensure it exists
    if (!std::filesystem::exists (dir))
    {
        if (!std::filesystem::create_directories (dir))
            return Error ({ErrorDomain::Conf, ErrorCode::DirectoryCreate}, {});
    }
    return dir;
}

ResNone Dispatch::setupLogs()
{
    auto resPath = getLogDir();
    if (!resPath)
        return resPath.Error();

    LogSinkInfo managedSink = {LogLevel::Debug};
    auto resLog = Log::The().AddManagedSink (managedSink, resPath.Value());
    if (!resLog)
        return resLog.Error();

    LogSinkInfo fileSink = {LogLevel::Debug};
    for (const auto& file : options.logFiles)
    {
        resLog = Log::The().AddFileSink (fileSink, file);
        if (!resLog)
            return resLog.Error();
    }
    return Success();
}

ResNone Dispatch::setupError()
{
    std::unique_ptr<ErrorFormatter> fmt = nullptr;
    if (options.traceErrors)
        fmt = std::make_unique<TraceErrorFormatter>();
    else
        fmt = std::make_unique<UserErrorFormatter> (options.verbose);

    std::unique_ptr<LogErrorSink> logSink = std::make_unique<LogErrorSink> (std::move (fmt));
    ErrorOutput::The()->AddSink (std::move (logSink));

    if (options.quiet)
        Log::The().SetSinkLogLevel (SinkType::Console, LogLevel::Max);
    else if (options.verbose)
        Log::The().SetSinkLogLevel (SinkType::Console, LogLevel::Debug);

    return Success();
}

ResNone Dispatch::selectImages (ImageSet& images)
{
    // If no selection was made on command line, return the whole set
    if (options.selectedImages.empty())
        return Success();

    return images.Filter (options.selectedImages);
}

// TODO for when we add in a configuration file for options
ResNone Dispatch::SetupConf()
{
    return Success();
}

bool Dispatch::Execute (OptionsParser& parser)
{
    // This is the main driver for the dispatcher. All the main business logic is controlled through here
    // This routine is a little annoying cause it's mostly error checking, but it is what it is
    auto resErr = setupError();
    if (!resErr)
    {
        // We can't do too much as we don't know if we have errOut avaiable, so just do our best
        Log::The().Fatal (resErr.Error().RootFrame().msg);
        return false;
    }

    auto resLog = setupLogs();
    if (!resLog)
    {
        dispatchFail (resLog.Error());
        return false;
    }

    // Invoke the frontend
    auto frontend = frontOpts.CreateFrontend (parser);
    auto resFront = frontend->Parse();
    if (!resFront)
    {
        dispatchFail (resFront.Error());
        return false;
    }
    auto& imageSet = frontend->GetSet();

    // Now we need to perform image selection
    auto resImage = selectImages (imageSet);
    if (!resImage)
    {
        dispatchFail (resImage.Error());
        return false;
    }

    // We now have the image set to operate on, now prepare the operation
    auto resOp = Operation::MakeOperation (options.operation, opOptions);
    if (!resOp)
    {
        dispatchFail (resOp.Error());
        return false;
    }

    auto resTargets = resOp.Value()->PrepareTargets (imageSet.GetImages());
    if (!resTargets)
    {
        dispatchFail (resTargets.Error());
        return false;
    }
    // TODO: hand targets off to a TaskGraph runner once that wiring exists

    // Now verify that there are no unused options
    auto resOpts = parser.CheckUnusedOpts();
    if (!resOpts)
    {
        dispatchFail (resOpts.Error());
        return false;
    }

    return true;
}

void Dispatch::CollectOptions (OptionsParser& opts)
{
    // clang-format off
    opts.AddOptions ("Global", "dispatch") 
        ("operation", "Operation to perform", options.operation) 
        ("q,quiet", "Make program run silently", options.quiet) 
        ("verbose", "Prints out verbose messages", options.verbose)
        ("trace-errors", "Print errors in trace format",options.traceErrors)
        ("b,backend", "Specifies default backend to use\n"
            "If an image specified to be generated is incompatible\n"
            "will use default backend for that image type",
            options.defaultBackend)
        ("l,log-file",
            "Specifies file to use for logging purposes\n"
            "Can be specified multiple times\n"
            "or as a comma-seperated list",
            options.logFiles)
        ("i,images", "Specifies images to operate on", options.selectedImages);
    // clang-format on

    // Add operation argument
    opts.AddPositional ("operation");

    // Go ahead and mark our set as used as it always is
    opts.UseSet ("dispatch");

    // Add frontend options
    frontOpts.CollectOptions (opts);

    // Now add every action's options
    opOptions.CollectOptions (opts);
}

ResNone Dispatch::ValidateOptions()
{
    // Ensure an operation was passed
    if (options.operation.empty())
        return makeOptionError ("No operation specified");

    // Validate frontend
    auto res = frontOpts.ValidateOptions();
    if (!res)
        return res;

    // Check operations
    res = opOptions.ValidateOptions();
    if (!res)
        return res;

    return Success();
}
