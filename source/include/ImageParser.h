/*
    ConfParser.h - contains header for lexer/parser
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

#ifndef NNIMAGE_CONFPARSER_H
#define NNIMAGE_CONFPARSER_H

enum class TokenType
{
    Identifier,
    String,
    Number,
    NumId,
    Colon,
    Semicolon,
    Comma,
    Obrace,
    Ebrace,
    Eof,
    Error,
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

struct ConfNumId
{
    uint64_t num;
    std::string id;
};

struct ConfToken
{
    TokenType type;
    int line;
    std::variant<std::string, uint64_t, ConfNumId> val;
};

class ConfLexer
{
  public:
    ConfLexer (const std::string& file, std::string_view& fileData);
    std::unique_ptr<ConfToken> NextToken();
    const std::string NameFromToken (TokenType type);

  private:
    char readChar();
    char peekChar();
    void skipChar();
    void returnChar (char c);
    bool isCharId (char c);
    bool isCharNum (char c, int base);
    bool isCharSpace (char c);
    void prepareEof (ConfToken* tok);
    void lexError (LexError err, const std::string& extra);
    void lexWarn (LexWarning err, const std::string& extra);
    const std::string file;
    ConfToken* curTok;
    std::string_view fileData;    // File data (in UTF-8)
    int idx;                      // Index in file data
    int curLine;
    bool isAccepted;
    bool isEof;
    bool isError;
    char nextChar;
};

// Parser structures
enum class ConfType
{
    Id,
    Num,
    NumId,
    String
};

struct ConfVal
{
  public:
    bool IsType (ConfType type) const
    {
        if (this->type == type)
            return true;
        return false;
    }
    const std::string& GetString() const
    {
        assert (type == ConfType::Id || type == ConfType::String);
        return std::get<std::string> (val);
    }
    uint64_t GetInteger() const
    {
        assert (type == ConfType::Num);
        return std::get<uint64_t> (val);
    }
    const ConfNumId& GetNumId() const
    {
        assert (type == ConfType::NumId);
        return std::get<ConfNumId> (val);
    }
    bool IsBool() const
    {
        if (this->type != ConfType::Id)
            return false;
        const std::string& id = std::get<std::string> (val);
        if (id != "true" && id != "false")
            return false;
        return true;
    }
    bool GetBoolean() const
    {
        assert (type == ConfType::Id);
        const std::string& id = std::get<std::string> (val);
        if (id == "true")
            return true;
        else if (id == "false")
            return false;
        else
            throw std::runtime_error ("ConfVal::GetBoolean: ConfVal is not a boolean");
    }
    int GetLine() const
    {
        return line;
    }

  private:
    ConfType type;
    std::variant<std::string, uint64_t, ConfNumId> val;
    int line;
    // TODO: remove this. This is a relic from before I made ConfVal define it's own interface
    friend class ConfParser;
    friend class ImageConf;
};

struct ParseProp
{
    std::string name;
    int line;
    std::vector<ConfVal> values;
};

struct ParseBlock
{
    int line;
    std::string type;
    std::string name;
    std::unordered_map<std::string, ParseProp> props;
};

enum class ParseError
{
    DuplicateProp,
    UnexpectedToken
};

class ConfParser
{
  public:
    ConfParser (const std::string& file, std::string_view fileData);
    bool NextBlock (ParseBlock& block, bool& isEof);

  private:
    ConfLexer lexer;
    const std::string& file;
    std::unique_ptr<ConfToken> lastToken;
    std::unique_ptr<ConfToken> getToken (std::unique_ptr<ConfToken> oldToken);
    // Similar to above, but fails if token isn't expected type
    std::unique_ptr<ConfToken> expectToken (TokenType type, std::unique_ptr<ConfToken> oldToken);
    void tokenError (TokenType expected, TokenType got, int line);
    void parseError (ParseError error, int line, const std::string& extra, const std::string& extra2 = "");
};

#endif
