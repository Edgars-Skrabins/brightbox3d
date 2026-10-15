#pragma once

#include "Core/Guid.h"
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Reflection/TypeInfo.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace BrightBox3D::Reflection
{
    template <typename E>
    const EnumInfo& StaticEnum();

    template <typename T>
    struct IsVector : std::false_type
    {
    };

    template <typename T>
    struct IsVector<std::vector<T>> : std::true_type
    {
        using Element = T;
    };

    template <typename T>
    concept ReferenceLike = requires {
        { T::ReflectionKind } -> std::convertible_to<PropertyKind>;
        { T::ReflectionReferenceType() } -> std::convertible_to<const char*>;
    };

    template <typename T>
    concept AssetReferenceLike = requires {
        { T::ReflectionKind } -> std::convertible_to<PropertyKind>;
        { T::ReflectionAssetType } -> std::convertible_to<Assets::AssetType>;
    };

    template <typename T>
    concept ClassReferenceLike = requires {
        { T::ReflectionKind } -> std::convertible_to<PropertyKind>;
        { T::ReflectionBaseType() } -> std::convertible_to<const TypeInfo&>;
    };

    template <typename T>
    ValueDescriptor DescribeValue()
    {
        ValueDescriptor descriptor;
        descriptor.size = sizeof(T);
        if constexpr (std::is_same_v<T, bool>)
        {
            descriptor.kind = PropertyKind::Bool;
        }
        else if constexpr (std::is_enum_v<T>)
        {
            descriptor.kind = PropertyKind::Enum;
            descriptor.enumInfo = &StaticEnum<T>();
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            descriptor.kind = PropertyKind::Int32;
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            descriptor.kind = PropertyKind::UInt32;
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            descriptor.kind = PropertyKind::Int64;
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            descriptor.kind = PropertyKind::Float;
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            descriptor.kind = PropertyKind::Double;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            descriptor.kind = PropertyKind::String;
        }
        else if constexpr (std::is_same_v<T, Math::Vector2>)
        {
            descriptor.kind = PropertyKind::Vector2;
        }
        else if constexpr (std::is_same_v<T, Math::Vector3>)
        {
            descriptor.kind = PropertyKind::Vector3;
        }
        else if constexpr (std::is_same_v<T, Math::Vector4>)
        {
            descriptor.kind = PropertyKind::Vector4;
        }
        else if constexpr (std::is_same_v<T, Math::Quaternion>)
        {
            descriptor.kind = PropertyKind::Quaternion;
        }
        else if constexpr (std::is_same_v<T, Math::Color>)
        {
            descriptor.kind = PropertyKind::Color;
        }
        else if constexpr (std::is_same_v<T, Core::Guid>)
        {
            descriptor.kind = PropertyKind::Guid;
        }
        else if constexpr (ClassReferenceLike<T>)
        {
            static_assert(std::is_standard_layout_v<T>, "Class reference properties must be standard layout with a type name as first member");
            descriptor.kind = PropertyKind::ClassReference;
            descriptor.baseType = &T::ReflectionBaseType;
        }
        else if constexpr (AssetReferenceLike<T>)
        {
            static_assert(std::is_standard_layout_v<T>, "Asset reference properties must be standard layout with a Guid as first member");
            descriptor.kind = T::ReflectionKind;
            descriptor.assetType = T::ReflectionAssetType;
        }
        else if constexpr (ReferenceLike<T>)
        {
            static_assert(std::is_standard_layout_v<T>, "Reference properties must be standard layout with a Guid as first member");
            descriptor.kind = T::ReflectionKind;
            descriptor.referenceType = T::ReflectionReferenceType();
        }
        else
        {
            static_assert(sizeof(T) == 0, "Unsupported type for BrightProperty/BrightUIProperty");
        }
        return descriptor;
    }

    template <typename T>
    void DescribeProperty(PropertyInfo& property)
    {
        if constexpr (IsVector<T>::value)
        {
            using Element = typename IsVector<T>::Element;
            static_assert(!IsVector<Element>::value, "Nested arrays are not supported");
            static_assert(!std::is_same_v<Element, bool>, "std::vector<bool> is not supported; use std::vector<int32_t>");
            property.value.kind = PropertyKind::Array;
            property.value.size = sizeof(T);
            property.element = DescribeValue<Element>();
            property.array.size = [](const void* array) { return static_cast<const T*>(array)->size(); };
            property.array.resize = [](void* array, size_t count) { static_cast<T*>(array)->resize(count); };
            property.array.element = [](void* array, size_t index) -> void* { return &(*static_cast<T*>(array))[index]; };
            property.array.constElement = [](const void* array, size_t index) -> const void* { return &(*static_cast<const T*>(array))[index]; };
        }
        else
        {
            property.value = DescribeValue<T>();
        }
    }
}
