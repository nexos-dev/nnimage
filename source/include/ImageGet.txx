/*
    ImageGet.txx - contains image getter functions
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

#include "include/Image.h"

template <typename T>
ResCustom<std::optional<T>, ImageError> Image::Get (const std::string& name)
{
    ImgProp prop = ResolveProp (name);
    if (prop == ImgProp::Max)
        return ImageError (ErrorCode::InvalidImgProp, {{"prop_name", name}, {"name", spec.name}});
    return Get<T> (prop);
}

template <typename T>
ResCustom<std::optional<T>, ImageError> Image::Get (ImgProp prop)
{
    const auto& registry = getRegistry();
    auto it = registry.find (prop);
    if (it == registry.end())
    {
        it = baseRegistry.find (prop);
        if (it == baseRegistry.end())
        {
            return ImageError (ErrorCode::InvalidImgProp,
                {{"prop_name", GetPropName (prop)}, {"name", spec.name}});
        }
    }
    assert (it->second.getter);

    auto val = it->second.getter (*this);
    if (!val)
        return std::optional<T>{};
    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);
    return ImageError (ErrorCode::PropTypeMismatch, {{"prop_name", GetPropName (prop)}, {"name", spec.name}});
}

template <typename T>
ResCustom<std::optional<T>, ImageError> Partition::Get (const std::string& name)
{
    PartProp prop = ResolveName (name);
    if (prop == PartProp::Max)
        return ImageError (ErrorCode::InvalidPartProp, {{"prop_name", name}, {"name", spec.name}});
    return Get<T> (prop);
}

template <typename T>
ResCustom<std::optional<T>, ImageError> Partition::Get (PartProp prop)
{
    auto it = registry.find (prop);
    if (it == registry.end())
    {
        return ImageError (ErrorCode::InvalidPartProp,
            {{"prop_name", GetPropName (prop)}, {"name", spec.name}});
    }
    assert (it->second.getter);

    auto val = it->second.getter (*this);
    if (!val)
        return std::optional<T>{};
    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);
    return ImageError (ErrorCode::PropTypeMismatch, {{"prop_name", GetPropName (prop)}, {"name", spec.name}});
}
