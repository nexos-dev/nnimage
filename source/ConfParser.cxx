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

#include "include/ConfParser.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <sstream>

template <typename Derived, typename ConfKey>
Result<LexToken> ConfParser<Derived, ConfKey>::parseProp (LexToken& startTok, ConfProp& prop)
{
    assert (startTok.type == TokenType::Identifier);
    prop.name = std::get<std::string> (std::move (startTok.val));

    // Get the equals sign
    auto res = expectToken (TokenType::Equals);
    if (!res)
        return res.Error();

    // Now we need to get the value
    // This is where things vary up a little bit
    res = nextToken();
    if (!res)
        return res;
    LexToken curTok = std::move (res.Value());

    switch (curTok.type)
    {
        case TokenType::Identifier:
            res = setIdProp (prop, curTok);
            if (!res)
                return res;
            curTok = std::move (res.Value());
            break;
        case TokenType::Number: {
            // Grab it
            res = setNumberProp (prop, curTok);
            if (!res)
                return res;
            curTok = std::move (res.Value());
            break;
        }
        case TokenType::Obrace:
            res = setListProp (prop, curTok);
            if (!res)
                return res;
            curTok = std::move (res.Value());
            break;
        default:
            return unexpectedToken (curTok.type);
    }
    return curTok;
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::parseLoop (std::vector<ConfProp>& props)
{
    while (1)
    {
        // We always expect an ID since the start of every property is the name
        // And EOF is processed here as well
        auto res = nextToken();
        if (!res)
            return res.Error();
        LexToken tok = std::move (res.Value());

        if (tok.type == TokenType::Eof)
            break;
        else if (tok.type != TokenType::Identifier)
            return unexpectedToken (tok.type);

        // Parse it now
        ConfProp curProp;
        res = parseProp (tok, curProp);
        if (!res)
            return res.Error();
        props.push_back (std::move (curProp));
    }
    return Success();
}

template <typename Derived, typename ConfKey>
Result<LexToken> ConfParser<Derived, ConfKey>::setIdProp (ConfProp& prop, LexToken& tok)
{
    prop.val = std::get<std::string> (std::move (tok.val));
    return expectToken (TokenType::Semicolon);
}

template <typename Derived, typename ConfKey>
Result<LexToken> ConfParser<Derived, ConfKey>::setListProp (ConfProp& prop, LexToken& tok)
{
    ConfList list;
    while (1)
    {
        auto res = expectToken (TokenType::Identifier);
        if (!res)
            return res;
        tok = std::move (res.Value());
        // Add it
        list.push_back (std::get<std::string> (std::move (tok.val)));

        res = nextToken();
        if (!res)
            return res;
        tok = std::move (res.Value());
        // Must be a comma or an ebrace
        if (tok.type == TokenType::Comma)
            continue;    // To next item
        else if (tok.type == TokenType::Ebrace)
            break;    // ENd it
        else
            return Error (unexpectedToken (tok.type));
    }
    prop.val = std::move (list);
    return expectToken (TokenType::Semicolon);
}

template <typename Derived, typename ConfKey>
Result<LexToken> ConfParser<Derived, ConfKey>::setNumberProp (ConfProp& prop, LexToken& tok)
{
    // Narrow it to int, but first give a go at range checking it
    uint64_t val = std::get<uint64_t> (tok.val);
    if (val > INT32_MAX)
    {
        return Error ({ErrorDomain::Conf, ErrorCode::IntegerOutOfRange}, {});
    }
    prop.val = static_cast<int> (std::get<uint64_t> (tok.val));
    return expectToken (TokenType::Semicolon);
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::Parse()
{
    std::unique_lock<std::shared_mutex> lock (parseLock);
    std::vector<ConfProp> props;
    auto res = parseLoop (props);
    if (!res)
        return res.Error();

    // Now we need to go through the properties and call the appropriate setters
    for (const ConfProp& prop : props)
    {
        ConfKey key = getPropKey (prop.name);
        assert (key != ConfKey::None);
        // We have the key, now set it. Use the lock-free variant since Parse() already holds
        // parseLock for the entire operation
        auto res = setLocked (key, prop.val, true);
        if (!res)
            return res.Error();
    }

    return Success();
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::Set (ConfKey key, const ConfValue& val, bool overwrite)
{
    std::unique_lock<std::shared_mutex> lock (parseLock);
    return setLocked (key, val, overwrite);
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::setLocked (ConfKey key, const ConfValue& val, bool overwrite)
{
    auto& ctrl = this->getKeyRegistry()[key];
    // Check if key already exists and is overwritable
    if (!overwrite && (ctrl.getter (derived()).index() < static_cast<size_t> (ConfType::Max)))
    {
        return Error ({ErrorDomain::Conf, ErrorCode::ParseError},
            {{"message", std::format ("Attempt to write key \"{}\" and overwrite is not enabled", nameFromKey (key))}});
    }

    ConfType type = getValueType (val);
    if (type != ctrl.type)
    {
        return Error ({ErrorDomain::Conf, ErrorCode::ParseError},
            {{"message", std::format ("Type mismatch on key \"{}\"", nameFromKey (key))}});
    }

    // Add it to our list of keys if it isn't in there
    // TODO: this might be kind of inefficient
    auto it = std::find (foundKeys.begin(), foundKeys.end(), key);
    if (it == foundKeys.end())
        foundKeys.push_back (key);

    ctrl.setter (derived(), val);
    return Success();
}

template <typename Derived, typename ConfKey>
Result<bool> ConfParser<Derived, ConfKey>::Get (ConfKey key, ConfValue& val) const
{
    std::shared_lock<std::shared_mutex> lock (parseLock);    // Grab the lock for reading
    val = getKeyRegistry()[key].getter (derived());
    if (val.index() >= static_cast<size_t> (ConfType::Max))
        return false;
    return true;
}

template <typename Derived, typename ConfKey>
ResNone ConfParser<Derived, ConfKey>::Serialize (std::string& out) const
{
    std::stringstream data;
    // Go through every key
    for (ConfKey key : foundKeys)
    {
        // Get the name
        auto it = std::find_if (getNameToKey().begin(), getNameToKey().end(), [key] (const auto& p) {
            return p.second == key;
        });
        // Ensure we could find it
        assert (it != getNameToKey().end());
        const std::string& name = it->first;

        // Now get the value
        ConfValue val = getKeyRegistry()[key].getter (derived());
        assert (val.index() < static_cast<size_t> (ConfType::Max));

        // We have the name and the value. Now we need to write it
        data << name;
        data << " = ";
        switch (getValueType (val))
        {
            case ConfType::Int:
                data << std::get<int> (val);
                break;
            case ConfType::String:
                data << std::get<std::string> (val);
                break;
            case ConfType::List: {
                data << "{";
                ConfList& list = std::get<ConfList> (val);
                for (auto it = list.begin(); it != list.end(); it++)
                {
                    data << *it;
                    if (std::next (it) != list.end())
                        data << ", ";
                }
                data << "}";
            }
            default:
                assert (false);
        }
        data << ";\n";
    }
    // Return the data
    out = data.str();
    return Success();
}

#include "include/ConfTemplates.h"
