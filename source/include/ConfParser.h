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
#include "include/EnumArray.h"
#include "include/SimpleLexer.h"
#include "include/StringHash.h"
#include "MemoryMapped.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// NOTE: MUST BE SYNCED WITH ORDER OF ConfValue
enum class ConfType
{
    Int,
    String,
    List,
    Max
};

// KEEP SYNCED WITH ABOVE
using ConfList = std::vector<std::string>;
using ConfValue = std::variant<int, std::string, ConfList, std::monostate>;

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

template <typename Derived, typename ConfKey>
class ConfParser
{
  public:
    ConfParser() = default;
    ConfParser (std::string fileName, std::string data) : lexer (std::move (fileName), std::move (data))
    {}
    virtual ~ConfParser() = default;

    Result<bool> Get (ConfKey key, ConfValue& val);
    ResNone Set (ConfKey key, const ConfValue& val, bool overwrite = true);

    ResNone Parse();
    ResNone Serialize (std::string& out);

  protected:
    // CRTP function
    Derived& derived()
    {
        return *static_cast<Derived*> (this);
    }

    virtual const EnumArray<ConfKey, ConfInstance<Derived, ConfKey>, ConfKey::Max>& getKeyRegistry() = 0;
    virtual const std::unordered_map<std::string, ConfKey, StringHash, std::equal_to<>>& getNameToKey() = 0;

  private:
    ResNone readFile();
    ResNone parseLoop (std::vector<ConfProp>& props);
    Result<LexToken> parseProp (LexToken& startTok, ConfProp& out);
    Result<LexToken> setNumberProp (ConfProp& prop, LexToken& tok);
    Result<LexToken> setIdProp (ConfProp& prop, LexToken& tok);
    Result<LexToken> setListProp (ConfProp& prop, LexToken& tok);
    // Does the actual work of Set() without acquiring parseLoc
    ResNone setLocked (ConfKey key, const ConfValue& val, bool overwrite);

    SimpleLexer lexer;
    std::vector<ConfKey> foundKeys;
    mutable std::shared_mutex parseLock;

    Result<LexToken> expectToken (TokenType expected)
    {
        auto res = lexer.NextToken();
        if (!res)
            return res.Error();
        LexToken& tok = res.Value();
        if (tok.type != expected)
            return unexpectedToken (tok.type);
        return std::move (tok);
    }
    Result<LexToken> nextToken()
    {
        return lexer.NextToken();
    }

    Error unexpectedToken (TokenType type)
    {
        return Error ({ErrorDomain::Log, ErrorCode::ParseError}, "Unexpected token \"{}\"", lexer.NameFromToken (type));
    }

    ConfKey getPropKey (std::string_view name)
    {
        auto& nameToKey = getNameToKey();
        auto it = nameToKey.find (name);
        if (it == nameToKey.end())
            return ConfKey::None;
        return it->second;
    }

    const std::string& nameFromKey (ConfKey key)
    {
        auto& nameToKey = getNameToKey();
        auto it =
            std::find_if (nameToKey.begin(), nameToKey.end(), [&key] (const auto& pair) { return pair.second == key; });
        if (it == nameToKey.end())
        {
            throw ErrorException (
                Error ({ErrorDomain::Log, ErrorCode::Internal}, "Access to non-existant log control key"));
        }
        return it->first;
    }

    // Yes, I know this is bad practice, but this is the simplest way of doing this
    static ConfType getValueType (const ConfValue& val)
    {
        return static_cast<ConfType> (val.index());
    }
};

#endif
