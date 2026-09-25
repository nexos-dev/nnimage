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
#include "include/image/ImgComponent.h"

#include <any>
#include <optional>

template <typename T>
Result<std::optional<T>> Image::Get (std::string_view name) const
{
    return dispatchByName (name, spec.name, [&] (ImgProp prop) { return Get<T> (prop); });
}

template <typename T>
Result<std::optional<T>> Image::Get (ImgProp prop) const
{
    auto resGet = getInternal (prop);
    if (!resGet)
        return resGet.Error();

    auto val = resGet.Value();
    if (!val.has_value())
        return std::optional<T>{};

    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);

    return ImageError::Make (ErrorCode::PropTypeMismatch, {{"prop", GetPropName (prop)}});
}

template <typename T>
auto Image::GetComponent (this auto& self, CompType type) -> Result<ComponentPtr<T, decltype (self)>>
{
    using CompPtr = ComponentPtr<T, decltype (self)>;

    auto* component = self.getComponent (type);
    if (!component)
        return ImageError::Make (ErrorCode::CompNotLoaded, {{"name_suffix", ImageError::NameSuffix (self.spec.name)}});

    CompPtr typedComponent = dynamic_cast<CompPtr> (component);
    if (!typedComponent)
    {
        throw ErrorException (Error ({ErrorDomain::Image, ErrorCode::UnexpectedComponentType}, {}));
    }

    return typedComponent;
}

auto Image::resolveComponent (this auto& self, ImgProp prop)
    -> std::optional<decltype (self.getComponent (CompType::Max))>
{
    auto it = self.keyMap.find (prop);
    assert (it != self.keyMap.end());

    CompType owner = it->second;
    if (auto* comp = self.getComponent (owner))
        return comp;
    return std::nullopt;
}

template <typename T>
Result<std::optional<T>> Partition::Get (std::string_view name) const
{
    return dispatchByName (name, spec.name, [&] (PartProp prop) { return Get<T> (prop); });
}

template <typename T>
Result<std::optional<T>> Partition::Get (PartProp prop) const
{
    auto res = RegElement::Get (prop);
    if (!res)
        return res.Error();

    auto val = res.Value();
    if (!val.has_value())
        return std::optional<T>{};

    if (T* ptr = std::any_cast<T> (&*val))
        return std::optional<T> (*ptr);

    return ImageError::Make (ErrorCode::PropTypeMismatch, {{"prop", GetPropName (prop)}});
}
