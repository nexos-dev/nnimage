/*
    ImgBase.h - contains base of image property system
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

#ifndef IMGBASE_H
#define IMGBASE_H

#include "include/StringHash.h"
#include "include/image/ImgError.h"
#include "include/image/ImgProp.h"

#include <algorithm>
#include <any>
#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <variant>

template <typename T>
class GenericRef
{
  public:
    GenericRef (std::string name, T& comp, int line = -1) : name{std::move (name)}, line{line}, comp{comp}
    {}
    std::string_view GetName() const
    {
        return name;
    }
    int GetLine() const
    {
        return line;
    }
    T& GetComp()
    {
        return comp;
    }

  private:
    T& comp;
    std::string name;
    int line;
};

// Generic name-to-enum registry
template <typename Value>
class NameRegistry
{
  public:
    NameRegistry (std::initializer_list<std::pair<const std::string, Value>> values) : values{values}
    {}

    template <size_t N>
    NameRegistry (const std::array<std::pair<std::string_view, Value>, N>& entries)
    {
        for (const auto& [name, value] : entries)
            values.emplace (name, value);
    }

    Value Resolve (std::string_view name) const
    {
        auto it = values.find (name);
        if (it == values.end())
            return Value::Max;
        return it->second;
    }

    const std::string& GetName (Value value) const
    {
        auto it =
            std::find_if (values.begin(), values.end(), [value] (const auto& pair) { return pair.second == value; });
        assert (it != values.end());
        return it->first;
    }

  private:
    std::unordered_map<std::string, Value, StringHash, std::equal_to<>> values;
};

// Type wrapper for, e.g., "128MiB" -> (128*1024*1024)
class ImageNumId
{
  public:
    ImageNumId() = default;
    ImageNumId (size_t num, std::string mul) : num{num}, mul{std::move (mul)}
    {}
    ResNone Parse()
    {
        auto it = mulMap.find (mul);
        if (it == mulMap.end())
            return Error ({ErrorDomain::Image, ErrorCode::InvalidMultiplier}, {{"multiplier", mul}});

        if (__builtin_mul_overflow (num, it->second, &val))
            return Error ({ErrorDomain::Image, ErrorCode::SizeOverflow}, {});

        valid = true;
        return Success();
    }

    uint64_t Get() const
    {
        if (valid)
            return val;
        else
            throw std::runtime_error ("Access to uninitialized numid");
    }

  private:
    uint64_t val = 0;
    bool valid = false;
    size_t num = 0;
    std::string mul{};
    static const std::unordered_map<std::string_view, size_t> mulMap;
};

// HACK: used purely to differentiate between a quoted string and an ID in the variant
struct ImageId
{
    ImageId() = default;
    ImageId (std::string str) : id{std::move (str)}
    {}
    std::string operator()() const
    {
        return id;
    }
    explicit operator std::string() const
    {
        return id;
    }
    std::string Str() const
    {
        return id;
    }
    std::string_view View() const
    {
        return id;
    }

  private:
    std::string id{};
};

using ImageList = std::vector<ImageId>;

// Variant of all valid image value types
using ImageValType = std::variant<uint64_t, ImageId, std::string, ImageList, bool, ImageNumId, std::monostate>;
using ImageValIdx = std::size_t;

constexpr std::size_t ImageValSize = std::variant_size_v<ImageValType>;

struct LexToken;

// An image value
class ImageVal
{
  public:
    ImageVal() = default;
    ImageVal (ImageValType val, int line = -1) : val{std::move (val)}, line{line}
    {}
    template <typename T, typename = std::enable_if_t<std::is_constructible_v<ImageValType, T>>>
    ImageVal (T&& val, int line = -1) : val{std::forward<T> (val)}, line{line}
    {}

    bool IsEmpty() const
    {
        return std::holds_alternative<std::monostate> (val);
    }
    int GetLine() const
    {
        return line;
    }
    template <typename T>
    std::optional<T> Get() const
    {
        if (!std::holds_alternative<T> (val))
            return {};
        return std::get<T> (val);
    }
    ImageValIdx GetType() const
    {
        return val.index();
    }

    ImageVal Cast (ImageValIdx wantedType) const;

    bool IsInvalid()
    {
        return val.index() == GetTypeIndex<std::monostate>();
    }

    // This function gets the index of the specified type
    // It's constexpr as it's used a lot in registry tables to fill them out at compile time
    template <typename T>
    static constexpr ImageValIdx GetTypeIndex()
    {
        ImageValIdx idx = std::variant_npos;

        auto check = [&]<ImageValIdx... Is> (std::index_sequence<Is...>) {
            ((std::is_same_v<T, std::variant_alternative_t<Is, ImageValType>> ? idx = Is : 0), ...);
        };
        check (std::make_index_sequence<std::variant_size_v<ImageValType>>{});

        return idx;
    }

    static Result<ImageVal> FromToken (LexToken tok);

  private:
    ImageValType val = std::monostate{};
    int line = -1;

    static constexpr std::monostate Invalid = std::monostate{};
};

// Generic property setters/getters
template <typename T>
using PropSetter = ResNone (*) (T&, const ImageVal&);

template <typename T>
using PropGetter = std::optional<std::any> (*) (const T&);

template <typename T>
struct ConfItem
{
    ImageValIdx inputType;
    ImageValType defaultVal;
    PropSetter<T> setter;
    PropGetter<T> getter;
};

template <typename Key, typename Object>
using ConfRegistry = std::unordered_map<Key, ConfItem<Object>>;

template <typename Element, typename Property, typename Registry>
class RegElement
{
  public:
    virtual ~RegElement() = default;

    virtual ResNone Set (Property prop, const ImageVal& val);
    virtual Result<std::optional<std::any>> Get (Property prop) const;
    virtual Result<bool> IsSet (Property prop) const;
    virtual ResNone SetDefaults();

  protected:
    RegElement (ErrorCode invalidPropertyCode, ErrorCode missingPropertyCode)
        : invalidPropertyCode{invalidPropertyCode}, missingPropertyCode{missingPropertyCode}
    {}

    Element& element()
    {
        return static_cast<Element&> (*this);
    }

    const Element& element() const
    {
        return static_cast<const Element&> (*this);
    }

    static Error makeRegElementError (ErrorCode code, std::string_view elementName, std::string_view propName)
    {
        return ImageError::Make (code,
            {{"prop", std::string (propName)}, {"name_suffix", ImageError::NameSuffix (elementName)}});
    }

    bool hasProperty (Property prop) const
    {
        const auto& registry = getRegistry();
        return registry.find (prop) != registry.end();
    }

  private:
    virtual const Registry& getRegistry() const = 0;
    virtual std::string_view getRegElementName() const = 0;
    virtual std::string_view getPropName (Property prop) const = 0;

    ErrorCode invalidPropertyCode;
    ErrorCode missingPropertyCode;
};

#endif
