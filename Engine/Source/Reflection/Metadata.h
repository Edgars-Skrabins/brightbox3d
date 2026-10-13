#pragma once

#include "Core/Icon.h"

#include <cstdint>
#include <string_view>

namespace BrightBox3D::Reflection
{
    struct Category
    {
        constexpr Category() = default;
        constexpr explicit Category(std::string_view displayName) : name(displayName) {}

        constexpr bool IsSet() const { return !name.empty(); }

        std::string_view name;
    };

    enum class Edit : uint8_t
    {
        None,
        Anywhere,
        DefaultsOnly,
        InstanceOnly,
        Visible,
        Hidden
    };

    enum class UIAccess : uint8_t
    {
        None,
        Read,
        Write
    };

    struct ClassMetadata
    {
        Category category{};
        Core::Icon icon = Core::Icon::None;
        bool abstract = false;
    };

    struct PropertyMetadata
    {
        Edit edit = Edit::None;
        Category category{};
    };

    struct UIPropertyMetadata
    {
        UIAccess access = UIAccess::None;
    };

    struct EnumMetadata
    {
    };

    constexpr bool Validate(const ClassMetadata& metadata)
    {
        return metadata.category.name.data() == nullptr || metadata.category.IsSet();
    }

    constexpr bool Validate(const PropertyMetadata& metadata)
    {
        return metadata.category.name.data() == nullptr || metadata.category.IsSet();
    }

    constexpr bool Validate(const UIPropertyMetadata& metadata)
    {
        return metadata.access != UIAccess::None;
    }

    constexpr bool Validate(const EnumMetadata&)
    {
        return true;
    }

    namespace Categories
    {
    }

    namespace Specifiers
    {
        using Core::Icon;
        using Reflection::Edit;
        using Reflection::UIAccess;
        namespace Categories = ::BrightBox3D::Reflection::Categories;
    }
}
