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

template <typename T>
Result<T> ImageCmd::getToken (const std::string& val, const std::string& prop, TokenType type)
{
    // We use the full lexer so we handle edge cases cleanly
    SimpleLexer lex ("", val);
    auto resTok = lex.NextToken();
    if (!resTok.IsOk())
        return resTok.GetError().Add ({ErrorDomain::ImageConf, ErrorCode::BadArgument}, "Unable to process {}", prop);

    auto tok = std::move (resTok.GetValue());
    if (tok->type != type)
    {
        return Error ({ErrorDomain::ImageConf, ErrorCode::BadArgument}, "{} specified in invalid format", prop);
    }
    T result = std::get<T> (tok->val);

    // Before returning we need to double check that this is the last token to avoid potential security
    // exploits
    resTok = lex.NextToken();
    if (!resTok.IsOk())
        return resTok.GetError();    // Shouldn't happen
    if (resTok.GetValue()->type != TokenType::Eof)
    {
        return Error ({ErrorDomain::ImageConf, ErrorCode::BadArgument}, "extraneous token while processing {}", prop);
    }
    return result;
}

Result<ImageNumId> ImageCmd::getSize()
{
    auto resTok = getToken<LexNumId> (opts.imgSize, "size", TokenType::NumId);
    if (!resTok.IsOk())
        return resTok.GetError();

    ImageNumId numId (resTok.GetValue().num, resTok.GetValue().id);
    auto resParse = numId.Parse();
    if (!resParse.IsOk())
        return resParse.GetError();

    return numId;
}

ResNone ImageCmd::Parse()
{
    // Our job is to create just one image structure, as we only support one image on the command line
    // NOTE: name is empty as command line images are anonymous

    return Success();
}
