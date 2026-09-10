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
#include "include/ImgComponent.h"

#include <any>
#include <optional>

template <typename T>
ResCustom<std::optional<T>, ImageError> Image::Get (std::string_view name)
{
    return dispatchByName (name, spec.name, [&] (ImgProp prop) { return Get<T> (prop); });
}

template <typename T>
ResCustom<std::optional<T>, ImageError> Image::Get (ImgProp prop)
{
    auto compRes = resolveComponent (prop);
    if (!compRes.IsOk())
        return compRes.GetError();

    std::optional<std::any> val{};
    auto comp = compRes.GetValue();

    if (comp.has_value())
    {
        assert (*comp);

        auto getRes = (*comp)->Get (prop);
        if (!getRes.IsOk())
            return getRes.GetError();

        val = getRes.GetValue();
    }
    else
    {
        auto getRes = RegElement::Get (prop);
        if (!getRes.IsOk())
            return getRes.GetError();
        val = getRes.GetValue();
    }

    if (!val)
        return std::optional<T>{};

    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);

    return ImageError (ErrorCode::PropTypeMismatch, {{"prop", GetPropName (prop)}, {"name", spec.name}});
}

template <typename T>
ResCustom<T*, ImageError> Image::GetComponent (CompType type)
{
    // TODO: should assert or no?
    assert (comps[type]);

    T* component = dynamic_cast<T*> (comps[type].get());
    if (!component)
        return ImageError (ErrorCode::BadArgument, "Requested image component has an unexpected type");

    return component;
}

template <typename T>
ResCustom<std::optional<T>, ImageError> Partition::Get (const std::string& name)
{
    return dispatchByName (name, spec.name, [&] (PartProp prop) { return Get<T> (prop); });
}

template <typename T>
ResCustom<std::optional<T>, ImageError> Partition::Get (PartProp prop)
{
    auto res = RegElement::Get (prop);
    if (!res.IsOk())
        return res.GetError();

    auto val = res.GetValue();
    if (!val.has_value())
        return std::optional<T>{};

    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);

    return ImageError (ErrorCode::PropTypeMismatch, {{"prop", GetPropName (prop)}, {"name", spec.name}});
}
