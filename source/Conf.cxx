/*
    Conf.cxx - contains image configuration file processor
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

// clang-format off
#include "nnimage.h"
#include "include/ConfParser.h"
#include <iconv.h>
#include "MemoryMapped.h"
#include "include/Chardet.h"
#include "include/Iconv.h"

// clang-format on

// Opens up configuration file
bool ImageConf::openConfFile (std::string& out)
{
    MemoryMapped file;
    if (!file.open (confFile.fileName))
    {
        // Print the error
        parseError (ConfErrorType::SysError, 0, file.getError());
        return false;
    }
    std::string_view data = reinterpret_cast<const char*> (file.getData());
    const std::string& fileEnc = confFile.fileEnc;
    std::string enc;
    float confidence;
    Chardet chardet;
    if (!chardet.Detect (data, enc, confidence))
    {
        // If detection fails, either libchardet is not available or it couldn't determine the
        // encoding. Look  at the confidence to determine which
        if (confidence > 0.0)
        {
            parseError (ConfErrorType::SysError, 0, "unable to detect character set");
            return false;
        }
        else
        {
            // This means that libchardet is unavailable, so we will just assume UTF-8
            // We have no way of knowing what the file is and nine out of ten time it will be UTF-8
            // If the user does pass a different encoding, they will get rubbish output,
            // but they will have to specify it themselves
            enc = "UTF-8";
            confidence = 1.0;    // The irony of the confidence being the highest when we have no
                                 // clue what's going on
            _log->Warn ("unable to detect character set, assuming UTF-8. Pass -confenc to force an "
                        "encoding");
        }
    }
    // First check if user passed an encoding
    if (!fileEnc.empty())
    {
        // Warn user if detected and specified encoding differ. If we are not confident in
        // the detected encoding, don't worry about warning them
        // NOTE: if ASCII is the detected encoding we won't warn as ASCII is compatible with
        // essentially every other encoding known to man
        if (enc != fileEnc && confidence > 0.5 && enc != "ASCII")
        {
            // It's not an error, but we will warn the user
            std::string detectEnc = enc;
            _log->Warn ("specified character encoding of \"" + fileEnc +
                        "\" doesn't match detected encoding of \"" + detectEnc + "\"");
        }
    }
    // If this is UTF-8/ASCII, we are done
    if ((enc == "UTF-8" || enc == "ASCII") && confidence > 0.5)
        out = data;
    else
    {
        // If user didn't specify a character encoding and we couldn't accuratly detect one,
        // error out
        if (confidence <= 0.5)
        {
            parseError (ConfErrorType::SysError,
                        0,
                        "unable to detect accurately character set (guessed \"" + enc +
                            "\", pass option -confenc to force)");
            return false;
        }
        Iconv conv (enc, "UTF-8");
        if (!conv.Convert (data, out))
        {
            parseError (ConfErrorType::SysError,
                        0,
                        "unable to convert configuration file from \"" + enc + "\" to UTF-8");
            return false;
        }
    }
    return true;
}

void ImageConf::parseError (ConfErrorType error, int line, const std::string& extra)
{
    // Construct output string for error
    std::string out;
    out += confFile.fileName;
    out += ":";
    if (line)
    {
        out += std::to_string (line);
        out += ": ";
    }
    else
        out += " ";
    // Now figure out what the error is
    assert (error != ConfErrorType::Ok);
    switch (error)
    {
        case ConfErrorType::BadType:
            out += "Unrecognized block type \"" + extra + "\"";
            break;
        case ConfErrorType::NoName:
            out += "Name required on block type \"" + extra + "\"";
            break;
        case ConfErrorType::BadProp:
            out += "Unrecognized property \"" + extra + "\"";
            break;
        case ConfErrorType::ExtraVals:
            out += "Property \"" + extra + "\" only accepts one value";
            break;
        case ConfErrorType::PropRequired:
            out += "Property \"" + extra + "\" is required";
            break;
        case ConfErrorType::WrongType:
            out += "Wrong type on property \"" + extra + "\"";
            break;
        case ConfErrorType::UnrecognizedId:
            out += "Unrecognized identifier \"" + extra + "\"";
            break;
        case ConfErrorType::UnrecognizedVal:
            out += "Unrecognized value \"" + extra + "\"";
            break;
        case ConfErrorType::SysError:
            out += extra;
            break;
        case ConfErrorType::BadNumId:
            out += "Bad numid on property \"" + extra + "\"";
            break;
        default:
            break;
    }
    // Now write it out
    _log->Error (out);
}

bool ImageConf::ValidateProp (const ParseProp& prop, ConfType expectedType, int maxVal, ConfError& result)
{
    if (prop.values.size() > maxVal)
    {
        result = ConfError (ConfErrorType::ExtraVals, prop);
        return false;
    }
    if (prop.values[0].type != expectedType)
    {
        result = ConfError (ConfErrorType::WrongType, prop);
        return false;
    }
    return true;
}

ParseProp& ImageConf::GetProp (ParseBlock& block,
                               const std::string& name,
                               ConfType expectedType,
                               int maxVals,
                               ConfError& result)
{
    static ParseProp empty = {};
    auto& conf = block.props;
    auto it = conf.find (name);
    if (it == conf.end())
    {
        result.code = ConfErrorType::PropRequired;
        result.line = block.line;
        result.msg = name;
        return empty;
    }
    auto& prop = it->second;
    if (!ValidateProp (prop, expectedType, maxVals, result))
        return empty;
    return prop;
}

bool ImageConf::GetVal (const ParseProp& prop, ConfVal& out, int idx)
{
    if (idx >= prop.values.size())
        return false;
    out = prop.values[idx];
    return true;
}

void ImageConf::removeProp (ParseBlock& block, ParseProp& prop)
{
    block.props.erase (prop.name);
}

bool ImageConf::addPartitions (Image* img, const ParseProp& prop)
{
    // Start looping through values
    for (const ConfVal& val : prop.values)
    {
        // Check the type
        if (val.type != ConfType::Id)
        {
            parseError (ConfErrorType::WrongType, val.line, prop.name);
            return false;
        }
        PartRef ref = PartRef (val.GetString(), val.line);
        img->AddPartition (ref);
    }
    return true;
}

bool ImageConf::processImageBlock (ParseBlock& block)
{
    ConfError result;
    // Get the configuration
    const std::string& name = block.name;
    if (name.empty())
    {
        parseError (ConfErrorType::NoName, block.line, block.type);
        return false;
    }
    auto& conf = block.props;
    // First we need to find a type
    // All other values are dependent on the type
    ParseProp& typeProp = GetProp (block, "type", ConfType::Id, 1, result);
    if (result.code != ConfErrorType::Ok)
        return false;
    // Can't fail. I think
    ConfVal val;
    GetVal (typeProp, val, 0);
    assert (val.type == ConfType::Id);
    const std::string& type = std::get<std::string> (val.val);
    // Now that we have the type, we can now instatiate the image
    auto image = Image::ImageFactory (type, name);
    if (image == nullptr)
    {
        parseError (ConfErrorType::UnrecognizedId, typeProp.line, type);
        return false;
    }
    // Remove from property list for main parser
    removeProp (block, typeProp);
    // Now we need to iterate through every key,value pair in the block's property map
    for (auto& [key, prop] : block.props)
    {
        // Handle special key case
        if (key == "partitions")
        {
            if (!addPartitions (image.get(), prop))
                return false;
            continue;
        }
        // Get value from property
        ConfVal val;
        GetVal (prop, val, 0);
        image->SetConf (key, val, result);
        if (result.code != ConfErrorType::Ok)
        {
            parseError (result.code, result.line, result.msg);
            return false;
        }
    }
    // Now add to images list
    GetAction()->AddImage (std::move (image));
    return true;
}

bool ImageConf::processPartitionBlock (ParseBlock& block)
{
    ConfError result;
    const std::string& name = block.name;
    auto& conf = block.props;
    // Create the partition object
    std::unique_ptr<Partition> part = std::make_unique<Partition> (name);
    // Iterate through the properties
    for (auto& [key, prop] : conf)
    {
        ConfVal val;
        GetVal (prop, val, 0);
        // Set the key
        part->SetConf (key, val, result);
        if (result.code != ConfErrorType::Ok)
        {
            parseError (result.code, result.line, result.msg);
            return false;
        }
    }
    GetAction()->AddPartition (std::move (part));
    return true;
}

// Parser entry point
bool ImageConf::ParseFile()
{
    // Open the file
    std::string file;
    if (!openConfFile (file))
        return false;
    // Now parse it
    ConfParser parser (confFile.fileName, file);
    ParseBlock curBlock;
    bool eof = false;
    if (!parser.NextBlock (curBlock, eof))
        return false;
    bool result = true;
    while (!eof && result)
    {
        // Figure out what this block is
        if (curBlock.type == "image")
            result = processImageBlock (curBlock);
        else if (curBlock.type == "partition")
            result = processPartitionBlock (curBlock);
        else
        {
            parseError (ConfErrorType::BadType, curBlock.line, curBlock.type);
            return false;
        }
        if (result)
            result = parser.NextBlock (curBlock, eof);
    }
    // Now resolve all partitions
    if (result)
    {
        ConfError e;
        result = GetAction()->ResolvePartitions (e);
        if (e.code != ConfErrorType::Ok)
            parseError (e.code, e.line, e.msg);
    }
    return result;
}
