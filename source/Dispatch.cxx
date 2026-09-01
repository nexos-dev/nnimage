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

#include "nnimage.h"

Dispatch::Dispatch()
{
    // Initialize every action
    for (auto it = actionTable.begin(); it != actionTable.end(); it++)
    {
        ActionReg& action = *it;
        action.options = ActionOptions::MakeActOptions (actionTable.index (it));
    }
}

Result<std::filesystem::path> Dispatch::getLogDir()
{
    auto dir = getConfigDir() / "log";
    // Ensure it exists
    if (!std::filesystem::exists (dir))
    {
        if (!std::filesystem::create_directories (dir))
            return Error ({ErrorDomain::Conf, ErrorCode::PathError}, "unable to create log directories");
    }
    return dir;
}

ResNone Dispatch::setupLogs()
{
    auto resPath = getLogDir();
    if (!resPath.IsOk())
        return resPath.GetError();

    LogSinkInfo managedSink = {LogLevel::Debug};
    auto resLog = _log->AddManagedSink (managedSink, resPath.GetValue());
    if (!resLog.IsOk())
        return resLog.GetError();

    LogSinkInfo fileSink = {LogLevel::Debug};
    for (const auto& file : options.logFiles)
    {
        resLog = _log->AddFileSink (fileSink, file);
        if (!resLog.IsOk())
            return resLog.GetError();
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
        _log->SetSinkLogLevel (SinkType::Console, LogLevel::Max);
    else if (options.verbose)
        _log->SetSinkLogLevel (SinkType::Console, LogLevel::Debug);

    return Success();
}

// TODO for when we add in a configuration file for options
ResNone Dispatch::SetupConf()
{
    return Success();
}

bool Dispatch::Execute()
{
    // This is the main driver for the dispatcher. All the main business logic is controlled through here
    // This routine is a little annoying cause it's mostly error checking, but it is what it is
    auto resErr = setupError();
    if (!resErr.IsOk())
    {
        // We can't do too much as we don't know if we have errOut avaiable, so just do our best
        _log->Fatal (resErr.GetError().RootFrame().msg);
        return false;
    }

    auto resLog = setupLogs();
    if (!resLog.IsOk())
    {
        dispatchFail (resLog.GetError());
        return false;
    }

    auto imgRes = Image::ImageFactory ("mbr", "test");
    if (!imgRes.IsOk())
    {
        dispatchFail (imgRes.GetError());
        return false;
    }

    auto img = std::move (imgRes.GetValue());
    ImageNumId size = ImageNumId (512, "MiB");
    size.Parse();
    img->Set (ImgProp::Size, ImageVal (size));

    auto resGet = img->Get<int64_t> (ImgProp::Size);
    auto val = resGet.GetValue();
    int64_t sizeM = (*val) / (1024 * 1024);

    img->Set (ImgProp::BootMode, ImageVal (ImageId ("bios")));
    auto resGet2 = img->Get<BootMode> (ImgProp::BootMode);
    auto mode = *resGet2.GetValue();

    img->Set (ImgProp::MbrFile, ImageVal ("testf"));
    auto resGet3 = img->Get<std::string> (ImgProp::MbrFile);
    auto file = *resGet3.GetValue();

    return true;
}

void Dispatch::CollectOptions (cxxopts::Options& opts)
{
    // Make clang-format not destroy our beautiful formatting
    // clang-format off
    opts.add_options ("Global")
        ("operation", "Operation to perform", cxxopts::value<std::string> (options.operation))
        ("q,quiet", "Make program run silently", cxxopts::value<bool> (options.quiet))
        ("verbose", "Prints out verbose messages", cxxopts::value<bool> (options.verbose))
        ("trace-errors", "Print errors in trace format", cxxopts::value<bool> (options.traceErrors))
        ("b,backend", "Specifies default backend to use\n"
                      "If an image specified to be generated is incompatible\n"
                      "will use default backend for that image type",
                      cxxopts::value<std::string> (options.defaultBackend), "BACKEND")
        ("l,log-file", "Specifies file to use for logging purposes\n"
                       "Can be specified multiple times\n"
                       "or as a comma-seperated list", 
                        cxxopts::value<std::vector<std::string>>(options.logFiles), "FILES...");
    // clang-format on
    // Add operation argument
    opts.parse_positional ({"operation"});

    // Add frontend options
    frontOpts.CollectOptions (opts);

    // Now add every action's options
    for (auto it = actionTable.begin(); it != actionTable.end(); it++)
    {
        ActionReg& action = *it;
        action.options->CollectOptions (opts);
    }
}

ResNone Dispatch::ValidateOptions()
{
    std::string errMsg = "";
    // Ensure an operation was passed
    if (options.operation.empty())
        return makeOptionError ("No operation specified");

    // Validate frontend
    auto res = frontOpts.ValidateOptions();
    if (!res.IsOk())
        return res;

    // Check every action
    for (auto it = actionTable.begin(); it != actionTable.end(); it++)
    {
        ActionReg& action = *it;
        auto res = action.options->ValidateOptions();
        if (!res.IsOk())
            return res;
    }

    return Success();
}
