/*
    SimpleLexer.h - contains a simple dumb lexer
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

#ifndef SIMPLELEXER_H
#define SIMPLELEXER_H

#include "include/Error.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

enum class TokenType
{
    Identifier,
    String,
    Number,
    NumId,
    Colon,
    Semicolon,
    Slash,
    Comma,
    Obrace,
    Ebrace,
    Eof,
    Equals,
    True,
    False,
    None
};

enum class LexError
{
    UnexpectedEof,
    UnexpectedChar,
    InvalidNum,
    InvalidChar
};

enum class LexWarning
{
    InvalidEsc
};

struct LexNumId
{
    uint64_t num;
    std::string id;
};

struct LexToken
{
    TokenType type;
    std::variant<std::string, uint64_t, LexNumId, bool> val;
    SourceLoc loc;
};

class SimpleLexer
{
  public:
    SimpleLexer() = default;
    SimpleLexer (std::string file, std::string data);
    Result<LexToken> NextToken();
    static const char* NameFromToken (TokenType type);
    static const char* NameFromToken (const LexToken& tok);

  private:
    char readChar();
    char peekChar();
    void skipChar();
    void returnChar (char c);
    bool isCharId (char c);
    bool isCharNum (char c, int base);
    bool isCharSpace (char c);
    void prepareEof (LexToken& tok);
    Error lexError (LexError errCode, SourceLoc loc, std::string_view extra);
    void lexWarn (LexWarning errCode, SourceLoc loc, std::string_view extra);
    std::string_view fileName;
    std::string fileData;    // File data (in UTF-8)
    std::size_t idx;         // Index in file data
    int curLine;
    bool isAccepted;
    bool isEof;
    bool isError;
    char nextChar;
};

#endif
