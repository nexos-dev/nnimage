/*
    ImageParser.h - contains ImageParser header
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

#ifndef IMAGEPARSER_H
#define IMAGEPARSER_H

#include <optional>

#include "include/SimpleLexer.h"
#include "include/StringHash.h"
#include "include/image/ImgBase.h"

struct ImgParseProp
{
    std::string propName;
    ImageVal val;
    int line;
};

struct ImgParseBlock
{
    std::string type;
    std::string name;
    int line;
    std::unordered_map<std::string, ImgParseProp, StringHash, std::equal_to<>> props;
};

enum class ImgParseError
{
    UnexpectedToken,
    InvalidList,
    PropOverwrite,
    Max
};

class ImageParser
{
  public:
    ImageParser() = default;
    ImageParser (std::string fileName, std::string data) : lexer{std::move (fileName), std::move (data)}
    {}

    Result<std::optional<ImgParseBlock>> ParseBlock();

  private:
    Error parseError (ImgParseError error, std::string_view extra, std::string_view extra2, int line);
    void parseWarning (ImgParseError error, std::string_view extra, std::string_view extra2, int line);
    Result<ImgParseBlock> processBlock (LexToken& startTok);
    Result<ImgParseProp> processProp (LexToken& startTok);
    Result<ImageList> processList (LexToken first);

    Result<TokenType> peekToken()
    {
        if (nextTok.has_value())
            return (*nextTok).type;

        auto resTok = lexer.NextToken();
        if (!resTok)
            return resTok.Error();

        TokenType type = resTok.Value().type;
        nextTok = std::move (resTok.Value());
        return type;
    }

    Result<LexToken> nextToken()
    {
        if (nextTok.has_value())
        {
            LexToken tok = std::move (*nextTok);
            nextTok.reset();
            return tok;
        }

        return lexer.NextToken();
    }

    Result<LexToken> expectToken (TokenType type)
    {
        auto tokRes = nextToken();
        if (!tokRes)
            return tokRes.Error();

        LexToken tok = std::move (tokRes.Value());
        if (tok.type != type)
        {
            return parseError (ImgParseError::UnexpectedToken,
                lexer.NameFromToken (tok),
                lexer.NameFromToken (type),
                tok.line);
        }
        return tok;
    }

    bool isValType (TokenType type)
    {
        return type == TokenType::Identifier || type == TokenType::String || type == TokenType::Number ||
               type == TokenType::NumId || type == TokenType::False || type == TokenType::True;
    }

    template <typename T>
    T&& getTokValue (LexToken& tok)
    {
        return std::move (std::get<T> (tok.val));
    }

    template <typename T>
    T copyTokValue (LexToken& tok)
    {
        return std::get<T> (tok.val);
    }

    Error makeError (ErrorCode code, std::string msg, int line, ErrorSeverity severity = ErrorSeverity::Error)
    {
        return Error ({ErrorDomain::Image, code, ErrorLog::Normal, severity}, {{"message", std::move (msg)}})
            .AddContext ({{"file", std::string (lexer.GetFileName())}, {"line", std::to_string (line)}});
    }

    SimpleLexer lexer;

    std::optional<LexToken> nextTok;    // buffered token set by peekToken()
};

#endif
