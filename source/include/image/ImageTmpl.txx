/*
    ImageTmpl.txx - contains image templated functions
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
#include "include/image/ImgComponent.h"

#include <any>
#include <optional>

template <typename T>
Result<std::optional<T>> Image::Get (std::string_view name) const
{
    ImgProp prop = ResolveProp (name);
    if (prop == ImgProp::Max)
        return invalidImgProp (name);
    return Get<T> (prop);
}

template <typename T>
std::optional<T> Image::Get (ImgProp prop) const
{
    auto val = getInternal (prop);
    if (!val.has_value())
        return std::optional<T>{};

    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);

    throw ErrorException (ImageError::Make (ErrorCode::PropTypeMismatch, {{"prop", GetPropName (prop)}}));
}

template <typename T>
auto Image::GetComponent (this auto& self, CompType type) -> ComponentPtr<T, decltype (self)>
{
    auto* base = self.getComponent (type);
    if (!base)
        return nullptr;

    auto* component = dynamic_cast<ComponentPtr<T, decltype (self)>> (base);
    if (!component)
        throw ErrorException (ImageError::Make (ErrorCode::UnexpectedComponentType, {}));
    return component;
}

auto Image::resolveComponent (this auto& self, ImgProp prop) -> ComponentPtr<Component, decltype (self)>
{
    auto it = self.keyMap.find (prop);
    if (it == self.keyMap.end())
        throw std::out_of_range ("Image property enum value is not registered");

    CompType owner = it->second;
    if (owner == CompType::Max)
        return nullptr;
    return self.getComponent (owner);
}

template <typename T>
Result<std::optional<T>> Partition::Get (std::string_view name) const
{
    PartProp prop = ResolveName (name);
    if (prop == PartProp::Max)
        return invalidPartProp (name);

    return Get<T> (prop);
}

template <typename T>
std::optional<T> Partition::Get (PartProp prop) const
{
    auto val = RegElement::Get (prop);
    if (!val.has_value())
        return std::optional<T>{};

    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);

    throw ErrorException (ImageError::Make (ErrorCode::PropTypeMismatch, {{"prop", GetPropName (prop)}}));
}
