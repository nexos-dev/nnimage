/*
    ImageCmd.cxx - contains ImageCmd frontend
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
#include "include/SimpleLexer.h"
#include "include/KeyValue.h"

Result<LexToken> ImageCmd::getOneToken (std::string val)
{
    assert (!val.empty());
    SimpleLexer lexer ("", std::move (val));

    auto resTok = lexer.NextToken();
    if (!resTok)
        return resTok.Error();

    LexToken tok = std::move (resTok.Value());

    auto resEnd = assertTokenEnd (lexer);
    if (!resEnd)
        return resEnd.Error();

    return tok;
}

ResNone ImageCmd::assertIsEnd (LexToken& tok)
{
    if (tok.type != TokenType::Eof)
        return Error ({ErrorDomain::Option, ErrorCode::ExtraneousToken}, {});
    return Success();
}

ResNone ImageCmd::assertTokenEnd (SimpleLexer& lex)
{
    // Our job is to ensure we reached the end of the token stream to catch extra tokens
    // This helps security as I'm sure some crafty user could hack the program with extra tokens
    auto resTok = lex.NextToken();
    if (!resTok)
        return resTok.Error();

    return assertIsEnd (resTok.Value());
}

template <typename T>
Result<T> ImageCmd::getTokenValue (std::string val, TokenType type)
{
    auto resTok = getOneToken (std::move (val));
    if (!resTok)
        return resTok.Error();

    LexToken tok = std::move (resTok.Value());
    if (tok.type != type)
        return Error ({ErrorDomain::Option, ErrorCode::InvalidArgumentFormat}, {});

    return std::get<T> (tok.val);
}

Result<ImageVal> ImageCmd::convertStr (std::string val)
{
    assert (!val.empty());
    // This function is a very small parser. Basically, we usually only accept one token of any type
    // and then convert it to an ImageVal. The exception is the '/' token, which when delimiting IDs
    // indicates a file path. That gets converted to a string
    // NOTE: we don't have a good way of differentiating between a string and an ID since the user probably doesn't
    // quote all string types, so that's why ImageVal allows implicit casting from ImageId to std::string

    SimpleLexer lexer ("", std::move (val));

    auto resTok = lexer.NextToken();
    if (!resTok)
        return resTok.Error();

    LexToken token = std::move (resTok.Value());
    std::string filePath{};

    switch (token.type)
    {
        case TokenType::String:
        case TokenType::Number:
        case TokenType::NumId:
        case TokenType::False:
        case TokenType::True: {
            auto resEnd = assertTokenEnd (lexer);
            if (!resEnd)
                return resEnd.Error();
            break;    // Go ahead and do the conversion
        }
        case TokenType::Identifier: {
            // Peak at the next token to see if it is a file path marker
            auto resNext = lexer.NextToken();
            if (!resNext)
                return resNext.Error();
            LexToken& nextTok = resNext.Value();

            if (nextTok.type == TokenType::Slash)
                filePath = std::get<std::string> (token.val);    // This will fall through
            else
            {
                // Assert end
                auto resEnd = assertIsEnd (nextTok);
                if (!resEnd)
                    return resEnd.Error();
                break;    // Do the conversion
            }
            // fallthrough
        }
        case TokenType::Slash: {
            filePath += '/';    // Add the first slash

            while (true)
            {
                auto resTok = lexer.NextToken();
                if (!resTok)
                    return resTok.Error();
                token = std::move (resTok.Value());

                // Must be an ID
                if (token.type == TokenType::Identifier)
                {
                    filePath += std::get<std::string> (token.val);

                    resTok = lexer.NextToken();
                    if (!resTok)
                        return resTok.Error();
                    token = std::move (resTok.Value());

                    if (token.type == TokenType::Slash)
                        filePath += '/';
                    // If not, a slash, break out
                    else
                        break;
                }
                // Break out otherwise
                else
                    break;
            }

            // Ensure the current token is EOF
            auto resEnd = assertIsEnd (token);
            if (!resEnd)
                return resEnd.Error();

            return ImageVal (std::move (filePath));
        }
        default:
            return Error ({ErrorDomain::Option, ErrorCode::InvalidArgumentFormat}, {});
    }
    return ImageVal::FromToken (std::move (token));
}

ResNone ImageCmd::processId (Image& image, ImgProp prop, std::string val)
{
    if (val.empty())
        return Success();

    auto resTok = getTokenValue<std::string> (std::move (val), TokenType::Identifier);
    if (!resTok)
        return resTok.Error();

    auto resSet = image.Set (prop, ImageId (std::move (resTok.Value())));
    if (!resSet)
        return resSet.Error();

    return Success();
}

ResNone ImageCmd::processNumId (Image& image, ImgProp prop, std::string val)
{
    if (val.empty())
        return Success();

    auto resTok = getTokenValue<LexNumId> (std::move (val), TokenType::NumId);
    if (!resTok)
        return resTok.Error();

    // Conver lexer's numid into ImageNumId
    LexNumId numLex = resTok.Value();
    ImageNumId numId (numLex.num, numLex.id);
    auto resParse = numId.Parse();
    if (!resParse)
        return resParse.Error();

    auto resSet = image.Set (prop, numId);
    if (!resSet)
        return resSet.Error();

    return Success();
}

ResNone ImageCmd::processProps (Image& image)
{
    for (const auto& prop : opts.props)
    {
        auto valRes = KeyVal::Parse (prop);
        if (!valRes.has_value())
            return Error ({ErrorDomain::Option, ErrorCode::MalformedImageProperty}, {});

        const auto& vals = *valRes;
        // We can only have only value because cxxopts comma splits, ensure that
        assert (vals.size() == 1);

        // Convert the value into an ImageVal
        std::string_view value = vals[0].second;
        auto resVal = convertStr (std::string (value));
        if (!resVal)
            return resVal.Error();

        auto resSet = image.Set (vals[0].first, resVal.Value());
        if (!resSet)
            return resSet.Error();
    }
    return Success();
}

ResNone ImageCmd::processPartitions (Image& image)
{
    const auto& parts = opts.partSpecs.values;
    for (const auto& partSpec : parts)
    {
        // Create the partition
        auto partPtr = std::make_unique<Partition> ("");
        Partition& part = *partPtr;

        // Parse it
        auto resParse = KeyVal::Parse (partSpec);
        if (!resParse.has_value())
            return Error ({ErrorDomain::Option, ErrorCode::MalformedPartitionSpec}, {});

        const auto& vals = *resParse;

        if (vals.empty())
            return Error ({ErrorDomain::Option, ErrorCode::MalformedPartitionSpec}, {});

        for (const auto& partProp : vals)
        {
            // Convert to an image val
            auto resVal = convertStr (std::string (partProp.second));
            if (!resVal)
                return resVal.Error();

            // Set it
            auto resSet = part.Set (partProp.first, resVal.Value());
            if (!resSet)
                return resSet.Error();
        }

        // Add it
        image.AddPartition (std::move (partPtr));
    }

    return Success();
}

ResNone ImageCmd::Parse()
{
    // Our job is to create just one image structure, as we only support one image on the command line
    // NOTE: name is empty as command line images are anonymous
    auto imagePtr = std::make_unique<Image> ("");
    Image& image = *imagePtr;

    // First set size
    auto res = processNumId (image, ImgProp::Size, opts.imgSize);
    if (!res)
        return badArgument (res.Error(), "--size");

    // Now parse boot mode
    res = processId (image, ImgProp::BootMode, opts.bootMode);
    if (!res)
        return badArgument (res.Error(), "--bootmode");

    // Now type
    res = processId (image, ImgProp::PartType, opts.imgType);
    if (!res)
        return badArgument (res.Error(), "--type");

    // Now process every other image-level property
    res = processProps (image);
    if (!res)
        return badArgument (res.Error(), "--imgprop");

    // Finally, process all partitions
    res = processPartitions (image);
    if (!res)
        return badArgument (res.Error(), "--partition");

    // Add the image
    auto resAdd = addImage (std::move (imagePtr));
    if (!resAdd)
        return resAdd;

    return Success();
}
