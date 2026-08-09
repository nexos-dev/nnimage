/*
    ConfParser.h - contains a simple configuration file format parser
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

#ifndef CONFPARSER_H
#define CONFPARSER_H

#include "include/Error.h"
#include "include/SimpleLexer.h"
#include <functional>
#include <string>

// NOTE: MUST BE SYNCED WITH ORDER OF ConfValue
// I know this is the C programmer coming out in me, but this trickery is the cleanest way to do
// this without a lot of template boilerplate
enum class ConfType
{
    Int,
    String,
    List,
    Max
};

// KEEP SYNCED WITH ABOVE
using ConfList = std::vector<std::string>;
using ConfValue = std::variant<int, std::string, ConfList>;

template <typename Derived, typename ConfKey>
class ConfParser;

template <typename Derived, typename ConfKey>
using ConfSetter = std::function<void (Derived&, const ConfValue&)>;
template <typename Derived, typename ConfKey>
using ConfGetter = std::function<ConfValue (Derived&)>;

template <typename Derived, typename ConfKey>
struct ConfInstance
{
    ConfType type;
    ConfSetter<Derived, ConfKey> setter;
    ConfGetter<Derived, ConfKey> getter;
};

struct ConfProp
{
    std::string name;
    ConfValue val;
};

using TokenPtr = std::unique_ptr<LexToken>;

template <typename Derived, typename ConfKey>
class ConfParser
{
  public:
    ConfParser (std::ifstream& file);
    Result<bool> Get (ConfKey key, ConfValue& val);
    ResNone Set (ConfKey key, const ConfValue& val, bool overwrite = true);

    ResNone Parse();

  protected:
    // CRTP function
    Derived& derived()
    {
        return *static_cast<Derived*> (this);
    }

  private:
    ResNone parseLoop (std::vector<ConfProp>& props);
    Result<TokenPtr> parseProp (TokenPtr startTok, ConfProp& out);
    Result<TokenPtr> setNumberProp (ConfProp& prop, LexToken* tok);
    Result<TokenPtr> setIdProp (ConfProp& prop, TokenPtr tok);
    Result<TokenPtr> setListProp (ConfProp& prop, TokenPtr tok);

    SimpleLexer lexer;

    Result<TokenPtr> expectToken (TokenType expected)
    {
        auto res = lexer.NextToken();
        if (!res.IsOk())
            return res.GetError();
        TokenPtr tok = std::move (res.GetValue());
        assert (tok);
        if (tok->type != expected)
            return unexpectedToken (tok->type);
        return std::move (tok);
    }
    Result<TokenPtr> nextToken()
    {
        auto res = lexer.NextToken();
        if (!res.IsOk())
            return res.GetError();
        TokenPtr tok = std::move (res.GetValue());
        assert (tok);
        return std::move (tok);
    }

    Error unexpectedToken (TokenType type)
    {
        return Error ({ErrorDomain::Log, ErrorCode::ParseError},
                      "Unexpected token \"%s\"",
                      lexer.NameFromToken (type));
    }
    Error parseFailed()
    {
        return Error ({ErrorDomain::Log, ErrorCode::ParseError},
                      "Parsing log control file %s failed",
                      confPath);
    }

    ConfKey getPropKey (const std::string& name)
    {
        auto it = nameToKey.find (name);
        if (it == nameToKey.end())
            return ConfKey::None;
        return it->second;
    }

    const std::string& nameFromKey (ConfKey key)
    {
        auto& nameToKey = getNameToKey();
        auto it = std::find_if (nameToKey.begin(), nameToKey.end(), [&key] (const auto& pair) {
            return pair.second == key;
        });
        if (it == nameToKey.end())
        {
            throw ErrorException (Error ({ErrorDomain::Log, ErrorCode::Internal},
                                         "Access to non-existant log control key"));
        }
        return it->first;
    }

    virtual const EnumArray<ConfKey, ConfInstance<Derived, ConfKey>, ConfKey::Max>&
    getKeyRegistry() = 0;
    virtual const std::unordered_map<std::string, ConfKey>& getNameToKey() = 0;

    // Yes, I know this is bad practice, but this is the simplest way of doing this
    static ConfType getValueType (const ConfValue& val)
    {
        return static_cast<ConfType> (val.index());
    }
};

#endif
