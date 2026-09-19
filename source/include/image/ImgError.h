/*
    ImgError.h - contains ImageError type
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

#ifndef IMGERROR_H
#define IMGERROR_H

#include "include/Error.h"
#include "include/StringHash.h"

#include <any>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

using ImageErrProp = std::pair<std::string, std::any>;

class ImageError : public Error
{
  public:
    ImageError() = default;
    ImageError (ErrorCode code, std::initializer_list<ImageErrProp> args) : Error ({ErrorDomain::ImageConf, code}, "")
    {
        frame = &frames[frames.size() - 1];
        keys.insert (args.begin(), args.end());
        makeMessage (*frame);
    }
    ImageError (ErrorCode code, std::string msg) : Error ({ErrorDomain::ImageConf, code}, std::move (msg))
    {
        frame = &frames[frames.size() - 1];
    }

    ImageError& AddKey (std::initializer_list<ImageErrProp> args)
    {
        keys.insert (args.begin(), args.end());
        makeMessage (*frame);    // Reset the message
        return *this;
    }

  private:
    using ErrorKeyMap = std::unordered_map<std::string, std::any, StringHash, std::equal_to<>>;

    void makeMessage (ErrorFrame& frame);

    void assertKeys (const std::vector<std::string>& keys)
    {
        for (const auto& key : keys)
            assert (this->keys.find (key) != this->keys.end());
    }

    std::string_view getString (std::string_view key)
    {
        auto it = keys.find (key);
        assert (it != keys.end());
        return std::string_view (std::any_cast<const std::string&> (it->second));
    }
    std::string_view getString (ErrorKeyMap::iterator it)
    {
        return std::string_view (std::any_cast<const std::string&> (it->second));
    }

    // Helper for adding name to image error output. If image is anonymous, it will not add anything
    std::string getName()
    {
        std::string_view name = getString ("name");
        if (!name.empty())
        {
            std::string result;
            result.reserve (name.size() + 3);
            result.append (" \"");
            result.append (name);
            result.push_back ('"');
            return result;
        }
        return {};
    }

    template <typename T>
    T getValue (std::string_view key)
    {
        auto it = keys.find (key);
        assert (it != keys.end());
        return std::any_cast<T> (it->second);
    }

    ErrorFrame* frame;
    ErrorKeyMap keys;
};

using ImageResult = ResCustom<NoResult, ImageError>;

#endif
