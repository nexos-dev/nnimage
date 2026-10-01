/*
    ImageConf.cxx - contains ImageConf frontend
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

#include "include/Frontend.h"
#include "include/sys/TextReader.h"
#include "include/ImageParser.h"
#include "include/Image.h"

// Image parser

Error ImageParser::parseError (ImgParseError error, std::string_view extra, std::string_view extra2, SourceLoc loc)
{
    std::string msg;
    switch (error)
    {
        case ImgParseError::UnexpectedToken:
            msg = std::format ("Unexpected token \"{}\"", extra);
            if (!extra2.empty())
                msg += std::format (" (expected \"{}\")", extra2);
            break;
        case ImgParseError::InvalidList:
            msg = std::format ("Invalid token type \"{}\" specified for list", extra);
            break;
        default:
            assert (false);
    }

    return makeError (ErrorCode::ImgParseError, std::move (msg), std::move (loc));
}

void ImageParser::parseWarning (ImgParseError error, std::string_view extra, std::string_view extra2, SourceLoc loc)
{
    std::string msg;
    switch (error)
    {
        case ImgParseError::PropOverwrite:
            msg = std::format ("Duplicate property \"{}\" on block \"{}\"", extra, extra2);
            break;
        default:
            assert (false);
    }

    ErrorOutput::The()->Report (makeError (ErrorCode::ImgParseWarning, std::move (msg), loc, ErrorSeverity::Warning));
}

Result<ImageList> ImageParser::processList (LexToken first)
{
    ImageList list;
    LexToken cur = std::move (first);
    while (true)
    {
        // Only identifier lists are supported. Give a more descriptive error instead of just "unexpected token"
        // in that case
        if (isValType (cur.type) && cur.type != TokenType::Identifier)
            return parseError (ImgParseError::InvalidList, lexer.NameFromToken (cur), {}, cur.loc);

        else if (cur.type != TokenType::Identifier)
            return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (cur), {}, cur.loc);

        list.push_back (std::move (getTokValue<std::string> (cur)));

        auto resTok = nextToken();
        if (!resTok)
            return resTok.Error();
        cur = std::move (resTok.Value());

        // This must be a seperator
        if (cur.type == TokenType::Semicolon)
            break;    // Break out if a semicolon
        else if (cur.type != TokenType::Comma)
            return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (cur), "\";\" or \",\"", cur.loc);

        // Get the next list entry
        resTok = nextToken();
        if (!resTok)
            return resTok.Error();
        cur = std::move (resTok.Value());
    }
    return list;
}

Result<ImgParseProp> ImageParser::processProp (LexToken& startTok)
{
    ImgParseProp prop;
    prop.propName = getTokValue<std::string> (startTok);
    prop.loc = startTok.loc;

    // Next we need a colon
    auto resTok = expectToken (TokenType::Colon);
    if (!resTok)
        return resTok.Error();

    ImageVal val{};

    // Now process the value
    resTok = nextToken();
    if (!resTok)
        return resTok.Error();
    LexToken tok = std::move (resTok.Value());

    // Now determine if this is a list or not
    auto resPeek = peekToken();
    if (!resPeek)
        return resPeek.Error();

    if (resPeek.Value() == TokenType::Comma)
    {
        SourceLoc listLoc = tok.loc;
        auto resList = processList (std::move (tok));
        if (!resList)
            return resList.Error();
        val = ImageVal (resList.Value(), listLoc);
    }
    else
    {
        // Determine what kind of value this is
        switch (tok.type)
        {
            case TokenType::Identifier:
                val = ImageVal (ImageId (getTokValue<std::string> (tok)), tok.loc);
                break;
            case TokenType::String:
                val = ImageVal (getTokValue<std::string> (tok), tok.loc);
                break;
            case TokenType::Number:
                val = ImageVal (getTokValue<uint64_t> (tok), tok.loc);
                break;
            case TokenType::NumId: {
                LexNumId id = getTokValue<LexNumId> (tok);
                ImageNumId numId = ImageNumId (id.num, std::move (id.id));
                auto resNumId = numId.Parse();

                if (!resNumId)
                    return resNumId.Error().AddContext (tok.loc);
                val = ImageVal (numId, tok.loc);
                break;
            }
            case TokenType::True:
                val = ImageVal (true, tok.loc);
                break;
            case TokenType::False:
                val = ImageVal (false, tok.loc);
                break;
            default:
                return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (tok), {}, tok.loc);
        }
        // Require a semicolon now
        resTok = expectToken (TokenType::Semicolon);
        if (!resTok)
            return resTok.Error();
    }
    prop.val = std::move (val);

    return prop;
}

Result<ImgParseBlock> ImageParser::processBlock (LexToken& startTok)
{
    ImgParseBlock block;
    block.loc = startTok.loc;
    block.type = getTokValue<std::string> (startTok);

    // Next we require a name
    auto resTok = expectToken (TokenType::Identifier);
    if (!resTok)
        return resTok.Error();
    LexToken tok = std::move (resTok.Value());
    block.name = std::move (getTokValue<std::string> (tok));

    // Now we need an obrace
    resTok = expectToken (TokenType::Obrace);
    if (!resTok)
        return resTok.Error();

    // Now begins the properties
    while (true)
    {
        auto resTok = nextToken();
        if (!resTok)
            return resTok.Error();

        LexToken tok = std::move (resTok.Value());
        if (tok.type == TokenType::Ebrace)
            break;
        else if (tok.type != TokenType::Identifier)
            return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (tok), {}, tok.loc);

        auto resProp = processProp (tok);
        if (!resProp)
            return resProp.Error();

        ImgParseProp prop = std::move (resProp.Value());
        auto& props = block.props;

        // Property overwrite is allowed, but it's worth flagging
        auto it = props.find (prop.propName);
        if (it != props.end())
            parseWarning (ImgParseError::PropOverwrite, prop.propName, block.name, prop.loc);

        props.insert_or_assign (prop.propName, std::move (prop));
    }

    return block;
}

Result<std::optional<ImgParseBlock>> ImageParser::ParseBlock()
{
    auto resTok = nextToken();
    if (!resTok)
        return resTok.Error();
    LexToken tok = std::move (resTok.Value());

    if (tok.type == TokenType::Identifier)
    {
        auto resBlock = processBlock (tok);
        if (!resBlock)
            return resBlock.Error();
        return std::optional<ImgParseBlock> (std::move (resBlock.Value()));
    }
    else if (tok.type == TokenType::Eof)
        return std::optional<ImgParseBlock>{};
    else
        return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (tok), {}, tok.loc);
}

Result<std::string> ImageConf::readConfFile()
{
    std::filesystem::path fileName = opts.confFile;
    assert (!fileName.empty());

    // Read in the file
    std::string data;
    try
    {
        auto reader = TextReader (fileName, opts.confEnc);
        auto readRes = reader.Read();
        if (!readRes)
            return readRes.Error();

        data = std::move (readRes.Value());
    }
    catch (ErrorException& e)
    {
        return e.Error();
    }
    return data;
}

ResNone ImageConf::addPartitionNames (Image& img, const ImageVal& val)
{
    ImageList list;
    if (val.GetType() != ImageVal::GetTypeIndex<ImageList>())
    {
        // Try to cast it
        ImageVal newVal = val.Cast (ImageVal::GetTypeIndex<ImageList>());
        if (newVal.IsInvalid())
            return ImageError::MakeWithContext (ErrorCode::PropTypeMismatch, {{"prop", "partitions"}}, val.GetLoc());
        list = *newVal.Get<ImageList>();
    }
    else
        list = *val.Get<ImageList>();
    for (auto& part : list)
        images.AddPartRef (std::move (part.Str()), img, val.GetLoc());

    return Success();
}

Result<std::unique_ptr<Image>> ImageConf::createImage (ImgParseBlock block)
{
    auto image = std::make_unique<Image> (std::move (block.name));

    // Go through every property and apply it
    for (const auto& [name, prop] : block.props)
    {
        // Special case: partition list
        if (name == "partitions")
        {
            auto resAdd = addPartitionNames (*image, prop.val);
            if (!resAdd)
                return resAdd.Error();
        }
        else
        {
            auto resSet = image->Set (name, prop.val);
            if (!resSet)
                return resSet.Error().AddContext (prop.loc);
        }
    }

    // Go ahead and resolve deferred properties as much as we can now. There may be some left that don't get filled out
    // till later, but we need to resolve as much as we can now
    image->ResolveAllDeferred();

    return image;
}

Result<std::shared_ptr<Partition>> ImageConf::createPartition (ImgParseBlock block)
{
    auto part = std::make_shared<Partition> (std::move (block.name));

    for (const auto& [name, prop] : block.props)
    {
        auto resSet = part->Set (name, prop.val);
        if (!resSet)
            return resSet.Error().AddContext (prop.loc);
    }
    return part;
}

ResNone ImageConf::Parse()
{
    auto resRead = readConfFile();
    if (!resRead)
        return parseFailed (resRead.Error());

    parser = ImageParser (opts.confFile, std::move (resRead.Value()));

    while (true)
    {
        auto resBlock = parser.ParseBlock();
        if (!resBlock)
            return resBlock.Error();
        if (!resBlock.Value().has_value())    // CHeck for EOF
            break;
        auto block = std::move (*resBlock.Value());

        // Determine if this is a image or a partition object
        if (block.type == "image")
        {
            auto resImg = createImage (std::move (block));
            if (!resImg)
                return resImg.Error();

            auto resAdd = images.AddImage (std::move (resImg.Value()));
            if (!resAdd)
                return resAdd.Error();
        }
        else if (block.type == "partition")
        {
            auto resPart = createPartition (std::move (block));
            if (!resPart)
                return resPart.Error();

            auto resAdd = images.AddPartition (std::move (resPart.Value()));
            if (!resAdd)
                return resAdd.Error();
        }
        else
            return ImageError::MakeWithContext (ErrorCode::ImgInvalidBlock, {{"block", block.type}}, block.loc);
    }

    return Success();
}
