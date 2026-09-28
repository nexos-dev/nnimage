/*
    EnumBitSet.h - creates a bit set based of an enum class
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

#ifndef ENUMBITSET_H
#define ENUMBITSET_H

#include <type_traits>
#include <bitset>
#include <cstddef>
#include <initializer_list>

template <typename T>
concept EnumBitType = std::is_enum_v<T>;

template <EnumBitType Enum, Enum Size>
class EnumBitSet
{
  public:
    constexpr EnumBitSet() = default;

    constexpr EnumBitSet (Enum val)
    {
        set (val);
    }
    constexpr EnumBitSet (std::initializer_list<Enum> vals)
    {
        for (Enum val : vals)
            set (val);
    }

    // Conviniently allows for chained bit operations
    constexpr EnumBitSet& set (Enum val)
    {
        bits.set (getIdx (val));
        return *this;
    }
    constexpr EnumBitSet& reset (Enum val)
    {
        bits.reset (getIdx (val));
        return *this;
    }
    constexpr bool test (Enum val) const
    {
        return bits.test (getIdx (val));
    }

  private:
    constexpr size_t getIdx (Enum val) const
    {
        return static_cast<size_t> (val);
    }

    std::bitset<static_cast<size_t> (Size)> bits;
};

#endif
