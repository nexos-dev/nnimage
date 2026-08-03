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
#include "ConfParser.h"
#include <cassert>
#ifdef HAVE_CHARDET
#include <chardet/chardet.h>
#endif
#include <iconv.h>
#include <cstring>
#include <iostream>
#include "MemoryMapped.h"

// clang-format on

// Converts from source encoding to UTF-8
bool ImageConf::convertFileEnc (std::string_view data,
                                size_t dataSize,
                                std::string& out,
                                const char* enc)
{
    iconv_t cd = iconv_open ("UTF-8", enc);
    if (cd == (iconv_t) -1)
    {
        _log->SysError ("unable to open iconv: ");
        return false;
    }
    size_t inLen = dataSize;
    size_t outLen = inLen * 4;    // Max possible size

    // Prepare the strings
    char* outBuf = new char[outLen];    // Allocate output buffer
    char* outBase = outBuf;
    char* in = const_cast<char*> (data.data());

    size_t inLeft = inLen;
    size_t outLeft = outLen;
    // Convert it
    size_t result = iconv (cd, &in, &inLeft, &outBuf, &outLeft);
    if (result == -1)
    {
        _log->SysError ("unable to convert character sets: ");
        return false;
    }
    out.assign (outBase, outLen - outLeft);
    iconv_close (cd);
    return true;
}

// Opens up configuration file
bool ImageConf::openConfFile (MemoryMapped& file, std::string_view& contents)
{
    // First open the file up
    if (!file.open (this->fileName))
    {
        // Print the error
        parseError (ConfErrorType::SysError, 0, file.getError());
        return false;
    }
    // Create a string view from it
    std::string_view data = reinterpret_cast<const char*> (file.getData());
    size_t sz = file.mappedSize();
#ifdef HAVE_CHARDET
    // Get the encoding of it
    DetectObj* obj = detect_obj_init();
    // FIXME: maybe passing the whole buffer is overkill?
    if (detect (data.data(), &obj))
    {
        // Error occured
        parseError (ConfErrorType::SysError, 0, "unable to determine character set");
        detect_obj_free (&obj);
        return false;
    }
    const char* enc = obj->encoding;
    float confidence = obj->confidence;
#else
    // Just default to ASCII as non-ASCII characters should be rare (famous last words)
    const char* enc = "ASCII";
    float confidence = 1.0f;    // This might be foolish
#endif
    const std::string& fileEnc = GetAction()->GetOption (OptionId::ConfEnc);
    // First check if user passed an encoding
    if (!fileEnc.empty())
    {
        // Use this encoding
        const char* userEnc = fileEnc.c_str();
        // Warn user if detected and specified encoding differ. If we are not confident in
        // the detected encoding, don't worry about warning them
        // NOTE: if ASCII is the detected encoding we won't warn as ASCII is compatible with
        // essentially every other encoding known to man
        if (strcmp (enc, userEnc) != 0 && confidence > 0.5 && strcmp (enc, "ASCII") != 0)
        {
            // It's not an error, but we will warn the user
            std::string detectEnc = enc;
            _log->Warn ("specified character encoding of \"" + fileEnc +
                        "\" doesn't match detected encoding of \"" + detectEnc + "\"");
        }
    }
    // If this is UTF-8, we are done
    if ((!strcmp (enc, "UTF-8") || !strcmp (enc, "ASCII")) && confidence > 0.5)
        contents = std::move (data);
    else
    {
        // If user didn't specify a character encoding and we couldn't accuratly detect one, error
        // out
        if (confidence <= 0.5)
        {
            std::string encStr = enc;
            parseError (ConfErrorType::SysError,
                        0,
                        "unable to detect character set (guessed \"" + encStr +
                            "\", pass option -confenc to force)");
#ifdef HAVE_CHARDET
            detect_obj_free (&obj);
#endif
            return false;
        }
        // Now we can convert it
        std::string out;
        if (!convertFileEnc (data, sz, out, enc))
        {
#ifdef HAVE_CHARDET
            detect_obj_free (&obj);
#endif
            return false;
        }
        contents = out;
    }
#ifdef HAVE_CHARDET
    detect_obj_free (&obj);
#endif
    return true;
}

void ImageConf::parseError (ConfErrorType error, int line, const std::string& extra)
{
    // Construct output string for error
    std::string out;
    out += this->fileName;
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

bool ImageConf::ValidateProp (const ParseProp& prop,
                              ConfType expectedType,
                              int maxVal,
                              ConfError& result)
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
    if (!ValidateProp (prop, ConfType::Id, 1, result))
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

int ImageConf::GetNumVals (ParseProp& prop)
{
    return prop.values.size();
}

void ImageConf::RemoveProp (ParseBlock& block, ParseProp& prop)
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
    RemoveProp (block, typeProp);
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
    std::string_view file;
    MemoryMapped handle;
    // NOTE: we pass the handle so that the string_view's contents don't go out of scope.
    // It's an awful hack but I don't care
    if (!openConfFile (handle, file))
        return false;
    // Now parse it
    ConfParser parser (this->fileName, file);
    bool eof = false;
    ParseBlock curBlock;
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
