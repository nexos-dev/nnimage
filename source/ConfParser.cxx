/*
    ConfParser.cxx - contains parser for configuration files
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
// clang-format on

#include <cassert>
#include <string_view>

// Lexer
ConfLexer::ConfLexer (const std::string& file, std::string_view& data) : fileData{data}, file{file}
{
    curLine = 1;
    curTok = nullptr;
    idx = 0;
    isAccepted = false;
    nextChar = 0;
    isEof = false;
    isError = false;
}

void ConfLexer::lexError (LexError err, const std::string& extra)
{
    std::string msg;
    msg = file;
    msg += ":";
    // Add the line
    msg += std::to_string (curLine);
    msg += ": ";
    // Now add the code
    switch (err)
    {
        case LexError::InvalidChar:
            msg += "Invalid character \"" + extra + "\"";
            break;
        case LexError::InvalidNum:
            msg += "Invalid number \"" + extra + "\"";
            break;
        case LexError::UnexpectedChar:
            msg += "Unexpected character \"" + extra + "\"";
            break;
        case LexError::UnexpectedEof:
            msg += "Unexpected EOF";
            break;
    }
    _log->Error (msg);
    // Set token to error
    curTok->type = TokenType::Error;
    isAccepted = true;
    isError = true;
}

char ConfLexer::readChar()
{
    // Check if we have a buffered character
    if (nextChar)
    {
        char c = nextChar;
        nextChar = 0;
        return c;
    }
    // Check for EOF
    if (idx == fileData.size())
    {
        isEof = true;
        return '\0';
    }
    // Get from buffer
    char c = fileData[idx];
    idx++;
    return c;
}

char ConfLexer::peekChar()
{
    // Check if we have a buffered character
    if (nextChar)
        return nextChar;
    // Check for EOF
    if (idx == fileData.size())
    {
        isEof = true;
        return '\0';
    }
    // Get from buffer
    char c = fileData[idx];
    idx++;
    nextChar = c;
    return c;
}

void ConfLexer::skipChar()
{
    assert (nextChar);
    nextChar = 0;
}

void ConfLexer::returnChar (char c)
{
    nextChar = c;
}

bool ConfLexer::isCharId (char c)
{
    switch (c)
    {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
        case '_':
            return true;
        default:
            return false;
    }
}

bool ConfLexer::isCharNum (char c, int base)
{
    switch (c)
    {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return true;
        case 'A':
        case 'a':
        case 'B':
        case 'b':
        case 'C':
        case 'c':
        case 'D':
        case 'd':
        case 'E':
        case 'e':
        case 'F':
        case 'f':
            return base == 16;
        default:
            return false;
    }
}

bool ConfLexer::isCharSpace (char c)
{
    switch (c)
    {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
        case '\v':
        case '\f':
            return true;
        default:
            return false;
    }
}

const std::string ConfLexer::NameFromToken (TokenType type)
{
    switch (type)
    {
        case TokenType::Colon:
            return ":";
        case TokenType::Ebrace:
            return "}";
        case TokenType::Identifier:
            return "identifier";
        case TokenType::Number:
            return "number";
        case TokenType::NumId:
            return "numid";
        case TokenType::Obrace:
            return "{";
        case TokenType::Semicolon:
            return ";";
        case TokenType::String:
            return "string";
        case TokenType::Comma:
            return ",";
        case TokenType::Eof:
            return "EOF";
        case TokenType::Error:
            return "error";
        case TokenType::None:
            return "none";
    }
    return "\0";
}

void ConfLexer::prepareEof (ConfToken* tok)
{
    isAccepted = true;
    isEof = true;
    tok->type = TokenType::Eof;
}

std::unique_ptr<ConfToken> ConfLexer::NextToken()
{
    // Make a new token
    std::unique_ptr<ConfToken> tok = std::make_unique<ConfToken>();
    int base = 0;
    curTok = tok.get();
    tok->line = curLine;
    tok->type = TokenType::None;
    // Check for EOF
    if (isEof)
    {
        tok->type = TokenType::Eof;
        return tok;
    }
    // Check for error
    else if (isError)
    {
        tok->type = TokenType::Error;
        return tok;
    }
    // Now keep looping until it's accepted
    isAccepted = false;
    while (!isAccepted)
    {
        char c = readChar();
        // Decide what to do with this character
        switch (c)
        {
            case '\0':
                // Always accept EOF
                isEof = true;
                isAccepted = true;
                tok->type = TokenType::Eof;
                break;
            // Whitespace
            case '\t':
            case '\v':
            case '\f':
            case ' ':
                break;    // Ignore whitespace
            case '\r':
                if (peekChar() == '\n')
                    skipChar();    // Skip new line too
            // fallthrough
            case '\n':
                ++curLine;
                break;
            case '#':
                // This is a comment. Keep reading until we hit a newline
                do
                {
                    // Consume every character
                    c = readChar();
                    if (c == '\n')
                    {
                        ++curLine;
                        break;
                    }
                    else if (c == '\r')
                    {
                        if (peekChar() == '\n')
                            skipChar();
                        ++curLine;
                        break;
                    }
                    // Check for EOF
                    else if (c == '\0')
                    {
                        prepareEof (tok.get());
                        break;
                    }
                } while (1);
                break;
            // Single-characters
            case '{':
                tok->type = TokenType::Obrace;
                goto scharCommon;
            case '}':
                tok->type = TokenType::Ebrace;
                goto scharCommon;
            case ':':
                tok->type = TokenType::Colon;
                goto scharCommon;
            case ';':
                tok->type = TokenType::Semicolon;
                goto scharCommon;
            case ',':
                tok->type = TokenType::Comma;
                goto scharCommon;
            scharCommon:
                isAccepted = true;
                tok->line = curLine;
                break;
            // ID
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'g':
            case 'h':
            case 'i':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'n':
            case 'o':
            case 'p':
            case 'q':
            case 'r':
            case 's':
            case 't':
            case 'u':
            case 'v':
            case 'w':
            case 'x':
            case 'y':
            case 'z':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
            case 'H':
            case 'I':
            case 'J':
            case 'K':
            case 'L':
            case 'M':
            case 'N':
            case 'O':
            case 'P':
            case 'Q':
            case 'R':
            case 'S':
            case 'T':
            case 'U':
            case 'V':
            case 'W':
            case 'X':
            case 'Y':
            case 'Z':
            case '_': {
                // This is an identifier
                tok->type = TokenType::Identifier;
                tok->line = curLine;
                std::string id;    // Prepare a string
                while (isCharId (c))
                {
                    id += c;
                    c = readChar();
                }
                // Return last character to buffer
                returnChar (c);
                tok->val = id;
                isAccepted = true;
                break;
            }
            case '0':
                // Could just be a zero, also could be a hex/binary/octal
                base = 0;
                if (peekChar() == 'x')
                {
                    base = 16;
                    skipChar();
                }
                else if (peekChar() == 'b')
                {
                    base = 2;
                    skipChar();
                }
                else if (isCharNum (peekChar(), 10))
                {
                    base = 8;
                    skipChar();
                }
                else
                {
                    // Invalid number
                    std::string num = "0";
                    num += readChar();
                    lexError (LexError::InvalidNum, num);
                    return tok;
                }
                goto lexNum;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                base = 10;
            // fallthrough
            lexNum:
                tok->type = TokenType::Number;    // Tentative
                tok->line = curLine;
                std::string numStr;
                // Go through every character
                while (isCharNum (c, base))
                {
                    numStr += c;
                    c = readChar();
                }
                // Convert to number
                int64_t val = 0;
                try
                {
                    val = std::stoi (numStr, 0, base);
                }
                catch (const std::invalid_argument& e)
                {
                    lexError (LexError::InvalidNum, numStr);
                    return tok;
                }
                // Check what next character is
                if (isCharId (c))
                {
                    // This is a numid
                    // Now we need to lex the ID part
                    std::string id;
                    while (isCharId (c))
                    {
                        id += c;
                        c = readChar();
                    }
                    returnChar (c);
                    // Now add to token
                    tok->val = ConfNumId{val, id};
                    tok->type = TokenType::NumId;
                }
                else
                {
                    returnChar (c);
                    tok->val = val;
                }
                isAccepted = true;
                break;
            }
            case '\'':
            case '"': {
                // This is a string
                char oc = c;        // Needed later
                std::string str;    // String we are holding
                tok->line = curLine;
                tok->type = TokenType::String;
                // Now loop through the characters until we find matching quote
                c = readChar();
                while (c != oc)
                {
                    // Check for EOF
                    if (c == '\0')
                    {
                        // That's an error
                        lexError (LexError::UnexpectedEof, "");
                        return tok;
                    }
                    // Check for escape
                    else if (c == '\\')
                    {
                        // See what the next char is
                        char next = peekChar();
                        if (next == oc)    // Quote
                        {
                            str += oc;
                            skipChar();
                        }
                        // Whitespace escape
                        else if (next == 'n')
                        {
                            str += '\n';
                            skipChar();
                        }
                        else if (next == 'r')
                        {
                            str += '\r';
                            skipChar();
                        }
                        else if (next == 't')
                        {
                            str += '\t';
                            skipChar();
                        }
                        // Now escape actual whitespace
                        else if (isCharSpace (next))
                        {
                            if (next == '\n')
                            {
                                ++curLine;
                                skipChar();
                            }
                            else if (next == '\r')
                            {
                                ++curLine;
                                skipChar();
                                if (peekChar() == '\n')
                                    skipChar();
                            }
                        }
                        else if (next == '\0')
                        {
                            lexError (LexError::UnexpectedEof, "");
                            return tok;
                        }
                    }
                    else
                        str += c;
                    c = readChar();
                }
                tok->val = str;
                isAccepted = true;
                break;
            }
            default: {
                // Unrecognized character
                std::string cStr;
                cStr += c;
                lexError (LexError::InvalidChar, cStr);
                return tok;
            }
        }
    }
    return tok;
}

// Parser implementation

ConfParser::ConfParser (const std::string& file, std::string_view& fileData)
    : lexer{file, fileData}, file{file}
{
    lastToken = nullptr;
}

std::unique_ptr<ConfToken> ConfParser::getToken (std::unique_ptr<ConfToken> oldToken)
{
    // Set the last token
    lastToken = std::move (oldToken);
    auto tok = lexer.NextToken();
    if (tok->type == TokenType::Error)
        return nullptr;
    return tok;
}

std::unique_ptr<ConfToken> ConfParser::expectToken (TokenType type,
                                                    std::unique_ptr<ConfToken> oldToken)
{
    // Set last token
    lastToken = std::move (oldToken);
    auto token = lexer.NextToken();
    if (token->type == TokenType::Error)
        return nullptr;
    else if (token->type != type)
    {
        tokenError (type, token->type, token->line);
        return nullptr;
    }
    return token;
}

void ConfParser::tokenError (TokenType expected, TokenType got, int line)
{
    std::string msg = file + ":" + std::to_string (line) +
                      ": "
                      "unexpected token \"" +
                      lexer.NameFromToken (got) + "\"";
    msg += " after token \"" + lexer.NameFromToken (lastToken->type) + "\"";
    if (expected != TokenType::None)
        msg += ", expected token \"" + lexer.NameFromToken (expected) + "\"";
    _log->Error (msg);
}

// Main parser
bool ConfParser::NextBlock (ParseBlock& block, bool& isEof)
{
    isEof = false;
    // initialize block
    block.line = 0;
    block.name = "";
    block.type = "";
    block.props.clear();
    // Start parsing
    auto token = getToken (std::move (lastToken));
    if (!token)
        return false;
    // Make sure it's an ID
    if (token->type == TokenType::Identifier)
    {
        // Now we must parse the block
        // First get the type
        block.line = token->line;
        block.type = std::get<std::string> (token->val);
        // Now get the next token
        token = getToken (std::move (token));
        if (!token)
            return false;
        // Make sure it's valid
        if (token->type == TokenType::Identifier)
        {
            // We have a name
            block.name = std::get<std::string> (token->val);
            token = expectToken (TokenType::Obrace, std::move (token));
            if (!token)
                return false;
        }
        else if (token->type != TokenType::Obrace)
        {
            // We expect a obrace
            tokenError (TokenType::Obrace, token->type, token->line);
            return false;
        }
        size_t propIdx = 0;
        while (1)
        {
            // Now start parsing the properties
            token = getToken (std::move (token));
            if (!token)
                return false;
            else if (token->type == TokenType::Ebrace)
                break;
            // It has to be an ID now
            else if (token->type != TokenType::Identifier)
            {
                tokenError (TokenType::None, token->type, token->line);
                return false;
            }
            // We know we have an ID, now create a new property
            ParseProp prop;
            prop.line = token->line;
            prop.name = std::get<std::string> (token->val);
            // Now we need a colon
            token = expectToken (TokenType::Colon, std::move (token));
            if (!token)
                return false;
            // Now start processing the values
            while (1)
            {
                token = getToken (std::move (token));
                if (!token)
                    return false;
                ConfVal val;
                val.line = token->line;
                if (token->type == TokenType::Identifier)
                {
                    val.type = ConfType::Id;
                    val.val = token->val;
                }
                else if (token->type == TokenType::String)
                {
                    val.type = ConfType::String;
                    val.val = token->val;
                }
                else if (token->type == TokenType::Number)
                {
                    val.type = ConfType::Num;
                    val.val = token->val;
                }
                else if (token->type == TokenType::NumId)
                {
                    val.type = ConfType::NumId;
                    val.val = token->val;
                }
                else
                {
                    tokenError (TokenType::None, token->type, token->line);
                    return false;
                }
                // Add to array
                prop.values.push_back (val);
                // Now move to next
                token = getToken (std::move (token));
                if (!token)
                    return false;
                else if (token->type == TokenType::Semicolon)
                    break;
                else if (token->type == TokenType::Comma)
                    continue;
                else
                {
                    tokenError (TokenType::None, token->type, token->line);
                    return false;
                }
            }
            block.props[prop.name] = prop;
        }
    }
    else if (token->type == TokenType::Eof)
    {
        isEof = true;
        return true;
    }
    else
    {
        tokenError (TokenType::Identifier, token->type, token->line);
        return false;
    }
    return true;
}
