/*
    SimpleLexer.cxx - contains a simple dumb lexer
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

#include "include/SimpleLexer.h"

#include <cassert>
#include <utility>

SimpleLexer::SimpleLexer (const std::string& file, std::string fileData) : fileData{std::move (fileData)}, file{file}
{
    curLine = 1;
    curTok = nullptr;
    idx = 0;
    isAccepted = false;
    nextChar = 0;
    isEof = false;
    isError = false;
}

void SimpleLexer::lexError (Error& err, LexError errCode, const std::string& extra)
{
    std::string msg;
    if (!file.empty())
    {
        msg += file;
        msg += ":";
        // Add the line
        msg += std::to_string (curLine);
        msg += ": ";
    }
    // Now add the code
    switch (errCode)
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
    err.Add ({ErrorDomain::Conf, ErrorCode::LexError}, msg);
    // Unconditionally accept and also make sure any further lexer access gets caught
    isAccepted = true;
    isError = true;
}

void SimpleLexer::lexWarn (LexWarning err, const std::string& extra)
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
        case LexWarning::InvalidEsc:
            msg += "Invalid escape sequence \"" + extra + "\", ignoring";
            break;
    }
    ErrorOutput::The()->Report (
        Error ({ErrorDomain::Conf, ErrorCode::LexError, ErrorLog::Normal, ErrorSeverity::Warning}, msg));
}

char SimpleLexer::readChar()
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

char SimpleLexer::peekChar()
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

void SimpleLexer::skipChar()
{
    assert (nextChar);
    nextChar = 0;
}

void SimpleLexer::returnChar (char c)
{
    nextChar = c;
}

bool SimpleLexer::isCharId (char c)
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

bool SimpleLexer::isCharNum (char c, int base)
{
    switch (c)
    {
        case '0':
        case '1':
            return true;
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            return base >= 8;
        case '8':
        case '9':
            return base >= 10;
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

bool SimpleLexer::isCharSpace (char c)
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

const char* SimpleLexer::NameFromToken (TokenType type)
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
        case TokenType::Equals:
            return "=";
        case TokenType::True:
            return "true";
        case TokenType::False:
            return "false";
        case TokenType::Eof:
            return "EOF";
        case TokenType::None:
            return "none";
    }
    return "\0";
}

void SimpleLexer::prepareEof (LexToken* tok)
{
    isAccepted = true;
    isEof = true;
    tok->type = TokenType::Eof;
}

using TokenResult = Result<std::unique_ptr<LexToken>>;

TokenResult SimpleLexer::NextToken()
{
    // Make a new token
    std::unique_ptr<LexToken> tok = std::make_unique<LexToken>();
    Error err;
    int base = 0;
    curTok = tok.get();
    tok->line = curLine;
    tok->type = TokenType::None;
    // Check for EOF
    if (isEof)
    {
        tok->type = TokenType::Eof;
        return TokenResult (std::move (tok));
    }
    assert (!isError);
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
            case '=':
                tok->type = TokenType::Equals;
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
                // Check if the ID is a reserved word
                if (id == "true")
                    tok->type = TokenType::True;
                else if (id == "false")
                    tok->type = TokenType::False;
                else
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
                    base = 10;    // This is a lone 0
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
                uint64_t val = 0;
                try
                {
                    val = std::stoull (numStr, 0, base);
                }
                catch (const std::invalid_argument& e)
                {
                    lexError (err, LexError::InvalidNum, numStr);
                    return TokenResult (err);
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
                    tok->val = LexNumId{val, id};
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
                        lexError (err, LexError::UnexpectedEof, "");
                        return TokenResult (err);
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
                            lexError (err, LexError::UnexpectedEof, "");
                            return TokenResult (err);
                        }
                        else
                        {
                            // Invalid escape
                            std::string esc;
                            esc += next;
                            lexWarn (LexWarning::InvalidEsc, esc);
                            skipChar();
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
                lexError (err, LexError::InvalidChar, cStr);
                return TokenResult (err);
            }
        }
    }
    return TokenResult (std::move (tok));
}
