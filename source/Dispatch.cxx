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
#include "include/Image.h"
#include "include/KeyValue.h"

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

void Dispatch::setupLogs()
{
    auto resPath = getLogDir();
    if (!resPath)
    {
        dispatchWarn (ErrorCode::LogInitFailed, {});
        return;
    }

    LogSinkInfo managedSink = {LogLevel::Debug};
    auto resLog = Log::The().AddManagedSink (managedSink, resPath.Value());
    if (!resLog)
        dispatchWarn (ErrorCode::LogInitFailed, {});

    LogSinkInfo fileSink = {LogLevel::Debug};
    for (const auto& file : options.logFiles)
    {
        resLog = Log::The().AddFileSink (fileSink, file);
        if (!resLog)
            dispatchWarn (ErrorCode::LogFileFailed, {{"file", file}});
    }
}

void Dispatch::setupError()
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
}

ResNone Dispatch::selectImages (ImageSet& images)
{
    // If no selection was made on command line, return the whole set
    if (options.selectedImages.empty())
        return Success();

    auto& selected = options.selectedImages;
    images.Filter ([&selected] (const auto& img) {
        auto it = selected.find (img.GetName());
        bool isFound = (it != selected.end());
        // If the image was selected, go ahead and remove it from the option. This simplifies the below logic
        if (isFound)
            selected.erase (it);
        return isFound;
    });

    // Any selections left were not found in the image set
    if (!selected.empty())
        return Error ({ErrorDomain::Operation, ErrorCode::ImgFilterFailed}, {{"name", *selected.begin()}});
    return Success();
}

ResNone Dispatch::createFileNames (ImageSet& images)
{
    if (!options.fileNames.empty())
    {
        // First, parse the command line specifications
        auto parsedSpec = KeyVal::Parse (options.fileNames);
        if (!parsedSpec && options.fileNames.size() == 1)
        {
            // The value was not a valid key=val format, but there's only was image name specified
            // If the image set only has one image, then we can just use what is there as the name
            if (images.size() != 1)
                return Error ({ErrorDomain::Operation, ErrorCode::ImgFileAmbiguous}, {{"text", options.fileNames[0]}});
            images.GetImages()[0].get().SetFilePath (options.fileNames[0]);
        }
        else
        {
            // Else go through each one and set it
            const auto& keyVals = *parsedSpec;
            for (const auto& [name, val] : keyVals)
            {
                auto image = images.FindImage (name);
                if (!image)
                    return Error ({ErrorDomain::Operation, ErrorCode::ImgNonExistant}, {{"name", std::string (name)}});
                image->get().SetFilePath (val);
            }
        }
    }

    // Now set default file names for all images that didn't have a specification
    for (auto& [name, image] : images)
    {
        if (image->GetFilePath().empty())
            setDefaultFile (*image);
    }
    return Success();
}

void Dispatch::setDefaultFile (Image& image)
{
    FormatComp* comp = image.GetComponent<FormatComp> (CompType::Format);
    if (!comp)
        return;    // validateImages will catch this

    std::string baseName = image.GetName() + comp->GetFileExt();
    image.SetFilePath (options.outPrefix + baseName);
}

ResNone Dispatch::validateImages (ImageSet& images)
{
    for (auto& [name, image] : images)
    {
        // If the operation is creating the image, then we need to set defaults
        if (op->CheckFlag (OpFlags::CreatesImage))
            image->SetDefaults();
        // Ensure we could get a file name
        if (image->GetFilePath().empty())
        {
            return ImageError::Make (ErrorCode::ImgFileNotSpecified,
                {{"name_suffix", ImageError::NameSuffix (*image)}});
        }

        // Ensure that there is a format specified on each image
        // TODO: we eventually are going to probe for format
        // however it is TBD wheter that is going to be merely for validation
        // or actually be able to set the format
        // One unknown is that we only can probe the format for images in which the file is explicity
        // specified, as default logic relies on knowing the format to determine the extension
        if (!image->CheckComponent (CompType::Format))
            return ImageError::Make (ErrorCode::ImgFormatRequired, {{"name_suffix", ImageError::NameSuffix (*image)}});

        // Now finalize the image
        auto resFinal = image->Finalize();
        if (!resFinal)
            return resFinal;
    }
    return Success();
}

// TODO for when we add in a configuration file for options
ResNone Dispatch::SetupConf()
{
    return Success();
}

bool Dispatch::Execute (OptionsParser& parser)
{
    setupError();
    setupLogs();

    // Invoke the frontend
    auto frontend = frontOpts.CreateFrontend (parser);
    auto resFront = frontend->Parse();
    if (!resFront)
        return dispatchFail (resFront.Error());
    auto& imageSet = frontend->GetSet();

    // Now we need to perform image selection
    auto resImage = selectImages (imageSet);
    if (!resImage)
        return dispatchFail (resImage.Error());

    // Now get the file names for each image
    auto resFile = createFileNames (imageSet);
    if (!resFile)
        return dispatchFail (resFile.Error());

    // We now have the image set to operate on, now prepare the operation
    auto resOp = Operation::MakeOperation (options.operation, opOptions);
    if (!resOp)
        return dispatchFail (resOp.Error());
    op = std::move (resOp.Value());

    // Validate all the images
    auto resValidate = validateImages (imageSet);
    if (!resValidate)
        return dispatchFail (resValidate.Error());

    auto resTargets = op->PrepareTargets (imageSet);
    if (!resTargets)
        return dispatchFail (resTargets.Error());

    // Now verify that there are no unused options.
    // Action instantiation occurs in PrepareTargets, so we can't do that until now
    auto resOpts = parser.CheckUnusedOpts();
    if (!resOpts)
        return dispatchFail (resOpts.Error());

    imageSet.Dump();

    // TODO: hand targets off to a TaskGraph runner once that wiring exists

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
        ("i,images", "Specifies images to operate on", options.selectedImages)
        ("o,out-image", "Specifies output image files", options.fileNames)
        ("out-prefix", "Output directory for image files\n"
                        "Ignored for images with an explicitly specified output file name",
                        options.outPrefix);
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
