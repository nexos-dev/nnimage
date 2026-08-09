/*
    BlockDevStr.h - contains block device string manager helper
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

#ifndef BLOCKDEVSTR_H
#define BLOCKDEVSTR_H

#include <string>

class BlockDevFactory
{
  public:
    BlockDevFactory() = default;
    BlockDevFactory (const std::string& prefix) : prefix (prefix), idx (0)
    {}

    std::string operator++ (int)
    {
        // Convert index to it's string form
        char suffix = 'a' + idx++;
        return prefix + suffix;
    }

    std::string operator++()
    {
        char suffix = 'a' + ++idx;
        return prefix + suffix;
    }

    std::string operator()()
    {
        char suffix = 'a' + idx;
        return prefix + suffix;
    }

    std::string operator() (int index)
    {
        char suffix = 'a' + index;
        return prefix + suffix;
    }

    std::string operator-- (int)
    {
        char suffix = 'a' + idx--;
        return prefix + suffix;
    }
    std::string operator--()
    {
        char suffix = 'a' + --idx;
        return prefix + suffix;
    }

  private:
    std::string prefix = "";
    int idx = 0;
};

#endif
