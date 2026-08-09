/*
    ConfParser.cxx - contains parser for configuration file format
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

#include "nnimage.h"

template <typename Derived, typename ConfKey>
Result<TokenPtr> ConfParser<Derived, ConfKey>::setIdProp (ConfProp& prop, TokenPtr tok)
{
    prop.val = std::get<std::string> (tok->val);
    return expectToken (TokenType::Semicolon);
}

template <typename Derived, typename ConfKey>
Result<TokenPtr> ConfParser<Derived, ConfKey>::setListProp (ConfProp& prop, TokenPtr tok)
{
    LogList list;
    while (1)
    {
        auto res = expectToken (TokenType::Identifier);
        if (!res.IsOk())
            return res;
        tok = std::move (res.GetValue());
        assert (tok);
        // Add it
        list.push_back (std::get<std::string> (tok->val));

        res = nextToken();
        if (!res.IsOk())
            return res;
        tok = std::move (res.GetValue());
        // Must be a comma or an ebrace
        if (tok->type == TokenType::Comma)
            continue;    // To next item
        else if (tok->type == TokenType::Ebrace)
            break;    // ENd it
        else
            return Error (unexpectedToken (tok->type));
    }
    prop.val = std::move (list);
    return expectToken (TokenType::Semicolon);
}

template <typename Derived, typename ConfKey>
Result<TokenPtr> ConfParser<Derived, ConfKey>::setNumberProp (ConfProp& prop, LexToken* tok)
{
    // Narrow it to int, but first give a go at range checking it
    uint64_t val = std::get<uint64_t> (tok->val);
    if (val > INT32_MAX)
    {
        return Error ({ErrorDomain::Log, ErrorCode::ParseError},
                      "Integer out of range",
                      logCtrlFile);
    }
    prop.val = static_cast<int> (std::get<uint64_t> (tok->val));
    return expectToken (TokenType::Semicolon);
}

template <typename Derived, typename ConfKey>
Result<TokenPtr> ConfParser<Derived, ConfKey>::parseProp (TokenPtr startTok, ConfProp& prop)
{
    TokenPtr curTok = std::move (startTok);
    assert (curTok->type == TokenType::Identifier);
    prop.name = std::get<std::string> (curTok->val);

    // Get the equals sign
    auto res = expectToken (TokenType::Equals);
    if (!res.IsOk())
        return res.GetError();
    curTok = std::move (res.GetValue());
    assert (curTok);

    // Now we need to get the value
    // This is where things vary up a little bit
    res = nextToken();
    if (!res.IsOk())
        return res;
    curTok = std::move (res.GetValue());
    assert (curTok);

    switch (curTok->type)
    {
        case TokenType::Identifier:
            res = setIdProp (prop, std::move (curTok));
            if (!res.IsOk())
                return res;
            curTok = std::move (res.GetValue());
            break;
        case TokenType::Number: {
            // Grab it
            res = setNumberProp (prop, curTok.get());
            if (!res.IsOk())
                return res;
            curTok = std::move (res.GetValue());
            break;
        }
        case TokenType::Obrace:
            res = setListProp (prop, std::move (curTok));
            if (!res.IsOk())
                return res;
            curTok = std::move (res.GetValue());
            break;
        default:
            return unexpectedToken (curTok->type);
    }
    return std::move (curTok);
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::parseLoop (std::vector<ConfProp>& props)
{
    TokenPtr tok = nullptr;
    while (1)
    {
        // We always expect an ID since the start of every property is the name
        // And EOF is processed here as well
        auto res = nextToken();
        if (!res.IsOk())
            return res.GetError();
        tok = std::move (res.GetValue());
        assert (tok);

        if (tok->type == TokenType::Eof)
            break;
        else if (tok->type != TokenType::Identifier)
            return unexpectedToken (tok->type);

        // Parse it now
        ConfProp curProp;
        res = parseProp (std::move (tok), curProp);
        if (!res.IsOk())
            return res.GetError();
        props.push_back (curProp);

        tok = std::move (res.GetValue());
        assert (tok);
    }
    return Success();
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::Parse()
{
    // Initialize lexer
    lexer = SimpleLexer (logCtrlFile, confFile);
    // Now start the parser
    std::vector<ConfProp> props;
    auto res = parseLoop (props);
    logCtrlLock.Unlock();    // Unlock first
    if (!res.IsOk())
        return res.GetError().Add (parseFailed());

    // Now we need to go through the properties and call the appropriate setters
    for (const ConfProp& prop : props)
    {
        ConfKey key = getPropKey (prop.name);
        if (key == ConfKey::None)
        {
            return Error ({ErrorDomain::Log, ErrorCode::ParseError},
                          "Reference to non-existant key \"%s\"",
                          prop.name)
                .Add (parseFailed());
        }
        // We have the key, now set it
        auto res = Set (key, prop.val, true);
        if (!res.IsOk())
            return res.GetError().Add (parseFailed());
    }

    return Success();
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::Set (ConfKey key, const ConfValue& val, bool overwrite)
{
    auto& ctrl = keys[key];
    // Check if key already exists and is overwritable
    if (!overwrite && (ctrl.getter (this).index() != std::variant_npos))
    {
        return Error ({ErrorDomain::Log, ErrorCode::ParseError},
                      "Attempt to write key \"%s\" and overwrite is not enabled",
                      nameFromKey (key));
    }
    ConfType type = getValueType (val);
    if (type != ctrl.type)
    {
        return Error ({ErrorDomain::Log, ErrorCode::ParseError},
                      "Type mismatch on key \"%s\"",
                      nameFromKey (key));
    }
    ctrl.setter (this, val);
    return Success();
}

template <typename Derived, typename ConfKey>
Result<bool> ConfParser<Derived, ConfKey>::Get (ConfKey key, ConfValue& val)
{
    val = keys[key].getter (this);
    if (val.index() == std::variant_npos)
        return false;
    return true;
}

#include "include/ConfTemplates.h"
