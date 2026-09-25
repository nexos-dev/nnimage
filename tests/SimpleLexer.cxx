/*
    SimpleLexer.cxx - contains simple lexer test cases
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

#include "doctest.h"
#include "include/SimpleLexer.h"

#include <string>

// Small helper that lexes an entire buffer and returns every token, stopping at (and including) EOF.
// Asserts that lexing never fails - use this only for inputs that are expected to succeed.
static std::vector<LexToken> LexAll (const std::string& data, const std::string& file = "test.conf")
{
    SimpleLexer lexer (file, data);
    std::vector<LexToken> tokens;
    while (true)
    {
        auto res = lexer.NextToken();
        REQUIRE (res);
        LexToken tok = res.Value();
        tokens.push_back (tok);
        if (tok.type == TokenType::Eof)
            break;
    }
    return tokens;
}

/********************
 *
 * SimpleLexer test cases
 *
 *********************/

TEST_CASE ("SimpleLexer lexes an empty buffer as immediate EOF")
{
    auto tokens = LexAll ("");
    REQUIRE (tokens.size() == 1);
    CHECK (tokens[0].type == TokenType::Eof);
}

TEST_CASE ("SimpleLexer lexes whitespace-only buffers as EOF and repeated EOF afterwards")
{
    SimpleLexer lexer ("test.conf", "   \t\n\r\n  ");
    auto res = lexer.NextToken();
    REQUIRE (res);
    CHECK (res.Value().type == TokenType::Eof);

    // Calling again after EOF must keep returning EOF rather than erroring or crashing
    res = lexer.NextToken();
    REQUIRE (res);
    CHECK (res.Value().type == TokenType::Eof);
}

TEST_CASE ("SimpleLexer recognizes every single-character token")
{
    auto tokens = LexAll ("{}:;,=");
    REQUIRE (tokens.size() == 7);
    CHECK (tokens[0].type == TokenType::Obrace);
    CHECK (tokens[1].type == TokenType::Ebrace);
    CHECK (tokens[2].type == TokenType::Colon);
    CHECK (tokens[3].type == TokenType::Semicolon);
    CHECK (tokens[4].type == TokenType::Comma);
    CHECK (tokens[5].type == TokenType::Equals);
    CHECK (tokens[6].type == TokenType::Eof);
}

TEST_CASE ("SimpleLexer lexes identifiers, including internal dashes and underscores")
{
    auto tokens = LexAll ("foo foo_bar foo-bar _leading");
    REQUIRE (tokens.size() == 5);
    CHECK (tokens[0].type == TokenType::Identifier);
    CHECK (std::get<std::string> (tokens[0].val) == "foo");
    CHECK (std::get<std::string> (tokens[1].val) == "foo_bar");
    CHECK (std::get<std::string> (tokens[2].val) == "foo-bar");
    CHECK (std::get<std::string> (tokens[3].val) == "_leading");
    CHECK (tokens[4].type == TokenType::Eof);
}

TEST_CASE ("SimpleLexer recognizes true/false as reserved keywords, not identifiers")
{
    auto tokens = LexAll ("true false truefalse");
    REQUIRE (tokens.size() == 4);
    CHECK (tokens[0].type == TokenType::True);
    CHECK (tokens[1].type == TokenType::False);
    // "truefalse" is its own identifier, not a concatenation of keywords
    CHECK (tokens[2].type == TokenType::Identifier);
    CHECK (std::get<std::string> (tokens[2].val) == "truefalse");
}

TEST_CASE ("SimpleLexer lexes decimal numbers")
{
    auto tokens = LexAll ("0 42 123456789");
    REQUIRE (tokens.size() == 4);
    CHECK (tokens[0].type == TokenType::Number);
    CHECK (std::get<uint64_t> (tokens[0].val) == 0);
    CHECK (std::get<uint64_t> (tokens[1].val) == 42);
    CHECK (std::get<uint64_t> (tokens[2].val) == 123456789);
}

TEST_CASE ("SimpleLexer lexes hexadecimal and binary numbers")
{
    auto tokens = LexAll ("0x1A 0xff 0b101 0b0");
    REQUIRE (tokens.size() == 5);
    CHECK (std::get<uint64_t> (tokens[0].val) == 0x1A);
    CHECK (std::get<uint64_t> (tokens[1].val) == 0xff);
    CHECK (std::get<uint64_t> (tokens[2].val) == 0b101);
    CHECK (std::get<uint64_t> (tokens[3].val) == 0);
}

TEST_CASE ("SimpleLexer parses octal literals without dropping the first digit after a leading zero")
{
    auto tokens = LexAll ("017");
    REQUIRE (tokens.size() == 2);
    CHECK (tokens[0].type == TokenType::Number);
    CHECK (std::get<uint64_t> (tokens[0].val) == 15);
}

TEST_CASE ("SimpleLexer reports an oversized integer as a lex error instead of throwing")
{
    SimpleLexer lexer ("test.conf", "99999999999999999999999999");
    auto res = lexer.NextToken();
    REQUIRE (!res);
    CHECK (res.Error().RootFrame().code == ErrorCode::LexError);
}

TEST_CASE ("SimpleLexer lexes a NumId as a number immediately followed by identifier characters")
{
    auto tokens = LexAll ("128MiB 4KiB");
    REQUIRE (tokens.size() == 3);
    CHECK (tokens[0].type == TokenType::NumId);
    LexNumId numId = std::get<LexNumId> (tokens[0].val);
    CHECK (numId.num == 128);
    CHECK (numId.id == "MiB");

    CHECK (tokens[1].type == TokenType::NumId);
    CHECK (std::get<LexNumId> (tokens[1].val).num == 4);
    CHECK (std::get<LexNumId> (tokens[1].val).id == "KiB");
}

TEST_CASE ("SimpleLexer treats a 0 num id correctly")
{
    // Zero is an edge case in the lexer logic, so check it in particular
    auto tokens = LexAll ("0GB");
    REQUIRE (tokens.size() == 2);
    CHECK (tokens[0].type == TokenType::NumId);
    CHECK (std::get<LexNumId> (tokens[0].val).num == 0);
    CHECK (std::get<LexNumId> (tokens[0].val).id == "GB");
}

TEST_CASE ("SimpleLexer NumId suffix can itself contain digits and dashes")
{
    auto tokens = LexAll ("1foo-2_bar3");
    REQUIRE (tokens.size() == 2);
    CHECK (tokens[0].type == TokenType::NumId);
    LexNumId numId = std::get<LexNumId> (tokens[0].val);
    CHECK (numId.num == 1);
    CHECK (numId.id == "foo-2_bar3");
}

TEST_CASE ("SimpleLexer lexes double- and single-quoted strings")
{
    auto tokens = LexAll (R"("hello world" 'single quoted')");
    REQUIRE (tokens.size() == 3);
    CHECK (tokens[0].type == TokenType::String);
    CHECK (std::get<std::string> (tokens[0].val) == "hello world");
    CHECK (tokens[1].type == TokenType::String);
    CHECK (std::get<std::string> (tokens[1].val) == "single quoted");
}

TEST_CASE ("SimpleLexer silently drops unrecognized escape sequences and warns instead of erroring")
{
    // "\q" is not a recognized escape (not the quote char, n/r/t, or whitespace), so the lexer emits a
    // warning via ErrorOutput and drops both the backslash and the following character entirely.
    auto tokens = LexAll (R"("a\qb")");
    REQUIRE (tokens.size() == 2);
    CHECK (tokens[0].type == TokenType::String);
    CHECK (std::get<std::string> (tokens[0].val) == "ab");
}

TEST_CASE ("SimpleLexer resolves a backslash-quote escape to a literal quote character")
{
    auto tokens = LexAll (R"("a\"b")");
    REQUIRE (tokens.size() == 2);
    CHECK (tokens[0].type == TokenType::String);
    CHECK (std::get<std::string> (tokens[0].val) == "a\"b");
}

TEST_CASE ("SimpleLexer resolves \\n \\r \\t escapes to real control characters")
{
    auto tokens = LexAll (R"("a\nb\rc\td")");
    REQUIRE (tokens.size() == 2);
    CHECK (std::get<std::string> (tokens[0].val) == "a\nb\rc\td");
}

TEST_CASE ("SimpleLexer allows an escaped literal newline to continue a string across lines")
{
    std::string data = "\"a\\\nb\"";    // "a\<newline>b" - escaped newline should be swallowed
    auto tokens = LexAll (data);
    REQUIRE (tokens.size() == 2);
    CHECK (std::get<std::string> (tokens[0].val) == "ab");
}

TEST_CASE ("SimpleLexer reports an unterminated string as a lex error")
{
    SimpleLexer lexer ("test.conf", "\"never closed");
    auto res = lexer.NextToken();
    REQUIRE_FALSE (res);
    CHECK (res.Error().RootFrame().domain == ErrorDomain::Conf);
    CHECK (res.Error().RootFrame().code == ErrorCode::LexError);
    CHECK (res.Error().RootFrame().msg.find ("Unexpected EOF") != std::string::npos);
}

TEST_CASE ("SimpleLexer reports an invalid character with file and line context")
{
    SimpleLexer lexer ("myfile.conf", "\n\n$");
    auto res = lexer.NextToken();
    REQUIRE_FALSE (res);
    CHECK (res.Error().MakeContextStr() == "myfile.conf:3: ");
    CHECK (res.Error().RootFrame().msg.find ("Invalid character \"$\"") != std::string::npos);
}

TEST_CASE ("SimpleLexer skips single-line comments and continues lexing")
{
    auto tokens = LexAll ("# this is a comment\nfoo # trailing comment\nbar");
    REQUIRE (tokens.size() == 3);
    CHECK (tokens[0].type == TokenType::Identifier);
    CHECK (std::get<std::string> (tokens[0].val) == "foo");
    CHECK (tokens[1].type == TokenType::Identifier);
    CHECK (std::get<std::string> (tokens[1].val) == "bar");
}

TEST_CASE ("SimpleLexer treats an unterminated trailing comment as EOF")
{
    auto tokens = LexAll ("foo # unterminated comment with no trailing newline");
    REQUIRE (tokens.size() == 2);
    CHECK (tokens[0].type == TokenType::Identifier);
    CHECK (tokens[1].type == TokenType::Eof);
}

TEST_CASE ("SimpleLexer tracks line numbers across newlines, comments and strings")
{
    std::string data = "foo\nbar\n# comment\nbaz";
    SimpleLexer lexer ("test.conf", data);

    auto res = lexer.NextToken();
    REQUIRE (res);
    CHECK (res.Value().line == 1);

    res = lexer.NextToken();
    REQUIRE (res);
    CHECK (res.Value().line == 2);

    res = lexer.NextToken();
    REQUIRE (res);
    CHECK (res.Value().line == 4);
}

TEST_CASE ("SimpleLexer handles CRLF line endings as a single newline")
{
    std::string data = "foo\r\nbar";
    SimpleLexer lexer ("test.conf", data);

    auto res = lexer.NextToken();
    REQUIRE (res);
    CHECK (res.Value().line == 1);

    res = lexer.NextToken();
    REQUIRE (res);
    CHECK (res.Value().line == 2);
}

TEST_CASE ("SimpleLexer::NameFromToken returns the expected display name for every token type")
{
    const SimpleLexer lexer ("test.conf", "");
    CHECK (lexer.GetFileName() == "test.conf");

    const LexToken token{TokenType::Identifier, 1, std::string{"name"}};
    CHECK (std::string (lexer.NameFromToken (token)) == "identifier");
    CHECK (std::string (lexer.NameFromToken (TokenType::Colon)) == ":");
    CHECK (std::string (lexer.NameFromToken (TokenType::Ebrace)) == "}");
    CHECK (std::string (lexer.NameFromToken (TokenType::Obrace)) == "{");
    CHECK (std::string (lexer.NameFromToken (TokenType::Semicolon)) == ";");
    CHECK (std::string (lexer.NameFromToken (TokenType::Comma)) == ",");
    CHECK (std::string (lexer.NameFromToken (TokenType::Equals)) == "=");
    CHECK (std::string (lexer.NameFromToken (TokenType::Identifier)) == "identifier");
    CHECK (std::string (lexer.NameFromToken (TokenType::Number)) == "number");
    CHECK (std::string (lexer.NameFromToken (TokenType::NumId)) == "numid");
    CHECK (std::string (lexer.NameFromToken (TokenType::String)) == "string");
    CHECK (std::string (lexer.NameFromToken (TokenType::True)) == "true");
    CHECK (std::string (lexer.NameFromToken (TokenType::False)) == "false");
    CHECK (std::string (lexer.NameFromToken (TokenType::Eof)) == "EOF");
    CHECK (std::string (lexer.NameFromToken (TokenType::None)) == "none");
}

TEST_CASE ("SimpleLexer lexes a realistic configuration snippet end to end")
{
    std::string data = R"(
        name = "my image";
        size = 128MiB;
        boot_mode = efi;
        readonly = true;
        tags = { alpha, beta, gamma };
    )";
    auto tokens = LexAll (data);

    // name = "my image";
    CHECK (tokens[0].type == TokenType::Identifier);
    CHECK (tokens[1].type == TokenType::Equals);
    CHECK (tokens[2].type == TokenType::String);
    CHECK (tokens[3].type == TokenType::Semicolon);
    // size = 128MiB;
    CHECK (tokens[4].type == TokenType::Identifier);
    CHECK (tokens[5].type == TokenType::Equals);
    CHECK (tokens[6].type == TokenType::NumId);
    CHECK (tokens[7].type == TokenType::Semicolon);
    // boot_mode = efi;
    CHECK (tokens[8].type == TokenType::Identifier);
    CHECK (tokens[9].type == TokenType::Equals);
    CHECK (tokens[10].type == TokenType::Identifier);
    CHECK (tokens[11].type == TokenType::Semicolon);
    // readonly = true;
    CHECK (tokens[12].type == TokenType::Identifier);
    CHECK (tokens[13].type == TokenType::Equals);
    CHECK (tokens[14].type == TokenType::True);
    CHECK (tokens[15].type == TokenType::Semicolon);
    // tags = { alpha, beta, gamma };
    CHECK (tokens[16].type == TokenType::Identifier);
    CHECK (tokens[17].type == TokenType::Equals);
    CHECK (tokens[18].type == TokenType::Obrace);
    CHECK (tokens[19].type == TokenType::Identifier);
    CHECK (tokens[20].type == TokenType::Comma);
    CHECK (tokens[21].type == TokenType::Identifier);
    CHECK (tokens[22].type == TokenType::Comma);
    CHECK (tokens[23].type == TokenType::Identifier);
    CHECK (tokens[24].type == TokenType::Ebrace);
    CHECK (tokens[25].type == TokenType::Semicolon);
    CHECK (tokens.back().type == TokenType::Eof);
}

TEST_CASE ("SimpleLexer stress test with a large number of tokens")
{
    std::string data;
    constexpr int count = 5000;
    for (int i = 0; i < count; i++)
        data += "field" + std::to_string (i) + " = " + std::to_string (i) + ";\n";

    auto tokens = LexAll (data);
    // Each entry produces 4 tokens (identifier, equals, number, semicolon) plus the trailing EOF
    REQUIRE (tokens.size() == static_cast<size_t> (count) * 4 + 1);
    CHECK (tokens[0].type == TokenType::Identifier);
    CHECK (std::get<std::string> (tokens[0].val) == "field0");
    CHECK (tokens.back().type == TokenType::Eof);
}
