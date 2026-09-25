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

Error ImageParser::parseError (ImgParseError error, std::string_view extra, std::string_view extra2, int line)
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

    return makeError (ErrorCode::ImgParseError, std::move (msg), line);
}

void ImageParser::parseWarning (ImgParseError error, std::string_view extra, std::string_view extra2, int line)
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

    ErrorOutput::The()->Report (makeError (ErrorCode::ImgParseWarning, std::move (msg), line, ErrorSeverity::Warning));
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
            return parseError (ImgParseError::InvalidList, lexer.NameFromToken (cur), {}, cur.line);

        else if (cur.type != TokenType::Identifier)
            return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (cur), {}, cur.line);

        list.push_back (std::move (getTokValue<std::string> (cur)));

        auto resTok = nextToken();
        if (!resTok)
            return resTok.Error();
        cur = std::move (resTok.Value());

        // This must be a seperator
        if (cur.type == TokenType::Semicolon)
            break;    // Break out if a semicolon
        else if (cur.type != TokenType::Comma)
            return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (cur), "\";\" or \",\"", cur.line);

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
    prop.line = startTok.line;

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
        int listLine = tok.line;
        auto resList = processList (std::move (tok));
        if (!resList)
            return resList.Error();
        val = ImageVal (resList.Value(), listLine);
    }
    else
    {
        // Determine what kind of value this is
        switch (tok.type)
        {
            case TokenType::Identifier:
                val = ImageVal (ImageId (getTokValue<std::string> (tok)), tok.line);
                break;
            case TokenType::String:
                val = ImageVal (getTokValue<std::string> (tok), tok.line);
                break;
            case TokenType::Number:
                val = ImageVal (getTokValue<uint64_t> (tok), tok.line);
                break;
            case TokenType::NumId: {
                LexNumId id = getTokValue<LexNumId> (tok);
                ImageNumId numId = ImageNumId (id.num, std::move (id.id));
                auto resNumId = numId.Parse();
                // TODO: hide this away
                if (!resNumId)
                {
                    return resNumId.Error().AddContext (
                        {{"file", std::string (lexer.GetFileName())}, {"line", std::to_string (tok.line)}});
                }
                val = ImageVal (numId);
                break;
            }
            case TokenType::True:
                val = ImageVal (true);
                break;
            case TokenType::False:
                val = ImageVal (false);
                break;
            default:
                return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (tok), {}, tok.line);
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
    block.line = startTok.line;
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
            return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (tok), {}, tok.line);

        auto resProp = processProp (tok);
        if (!resProp)
            return resProp.Error();

        ImgParseProp prop = std::move (resProp.Value());
        auto& props = block.props;

        // Property overwrite is allowed, but it's worth flagging
        auto it = props.find (prop.propName);
        if (it != props.end())
            parseWarning (ImgParseError::PropOverwrite, prop.propName, block.name, prop.line);

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
        return parseError (ImgParseError::UnexpectedToken, lexer.NameFromToken (tok), {}, tok.line);
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
    if (val.GetType() != ImageVal::GetTypeIndex<ImageList>())
    {
        return ImageError::MakeWithContext (ErrorCode::PropTypeMismatch,
            {{"prop", "partitions"}},
            parser.GetFileName(),
            val.GetLine());
    }

    ImageList list = *val.Get<ImageList>();
    for (auto& part : list)
        partRefs.push_back (GenericRef<Image> (std::move (part.Str()), img, val.GetLine()));

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
            {
                return resSet.Error().AddContext (
                    {{"file", std::string (parser.GetFileName())}, {"line", std::to_string (prop.line)}});
            }
        }
    }

    // Go ahead and resolve deferred properties as much as we can now. There may be some left that don't get filled out
    // till later, but we need to resolve as much as we can now
    image->ResolveDeferred();

    return image;
}

Result<std::shared_ptr<Partition>> ImageConf::createPartition (ImgParseBlock block)
{
    auto part = std::make_shared<Partition> (std::move (block.name));

    for (const auto& [name, prop] : block.props)
    {
        auto resSet = part->Set (name, prop.val);
        if (!resSet)
        {
            return resSet.Error().AddContext (
                {{"file", std::string (parser.GetFileName())}, {"line", std::to_string (prop.line)}});
        }
    }
    return part;
}

ResNone ImageConf::resolveImgRefs()
{
    // Go through each image
    for (const auto& image : images)
    {
        const auto& refs = image.second->GetRefs();

        for (const auto& ref : refs)
        {
            std::string_view name = ref.ref.GetName();

            // Find image with that name
            auto imageIt = images.find (name);
            if (imageIt == images.end())
            {
                return ImageError::MakeWithContext (ErrorCode::UnresolvedImage,
                    {{"image_name", std::string (name)}},
                    parser.GetFileName(),
                    ref.ref.GetLine());
            }

            ref.setter (imageIt->second.get());
        }
    }
    return Success();
}

ResNone ImageConf::resolvePartRefs()
{
    for (auto& ref : partRefs)
    {
        std::string_view partName = ref.GetName();

        auto partIt = partitions.find (partName);
        if (partIt == partitions.end())
        {
            return ImageError::MakeWithContext (ErrorCode::UnresolvedPartition,
                {{"part_name", std::string (partName)}},
                parser.GetFileName(),
                ref.GetLine());
        }
        ref.GetComp().AddPartition (partIt->second);
    }
    return Success();
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

            auto resAdd = addImage (std::move (resImg.Value()));
            if (!resAdd)
                return resAdd.Error();
        }
        else if (block.type == "partition")
        {
            auto resPart = createPartition (std::move (block));
            if (!resPart)
                return resPart.Error();

            auto resAdd = addPartition (std::move (resPart.Value()));
            if (!resAdd)
                return resAdd.Error();
        }
        else
        {
            return ImageError::MakeWithContext (ErrorCode::ImgInvalidBlock,
                {{"block", block.type}},
                parser.GetFileName(),
                block.line);
        }
    }

    // Now resolve all image and partition references
    auto resPartRef = resolvePartRefs();
    if (!resPartRef)
        return resPartRef.Error();

    auto resImgRef = resolveImgRefs();
    if (!resPartRef)
        return resPartRef.Error();

    return Success();
}
