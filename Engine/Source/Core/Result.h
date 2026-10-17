#pragma once

#include "Core/Assert.h"
#include "Core/Error.h"

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace BrightBox3D::Core
{
    template <typename T>
    concept PointerLike = std::is_pointer_v<T> || requires(const T& value) { value.operator->(); };

    template <typename T, typename E = Error>
    class [[nodiscard]] Result
    {
    public:
        template <typename U = T>
            requires(std::is_constructible_v<T, U &&> && !std::is_same_v<std::remove_cvref_t<U>, E> && !std::is_same_v<std::remove_cvref_t<U>, Result>)
        Result(U&& value) : m_state(std::in_place_index<0>, std::forward<U>(value))
        {
        }

        Result(E error) : m_state(std::in_place_index<1>, std::move(error)) {}

        bool HasValue() const { return m_state.index() == 0; }
        explicit operator bool() const { return HasValue(); }

        T& GetValue()
        {
            BrightAssert(HasValue(), "Result value read from an error");
            return std::get<0>(m_state);
        }

        const T& GetValue() const
        {
            BrightAssert(HasValue(), "Result value read from an error");
            return std::get<0>(m_state);
        }

        const E& GetError() const
        {
            BrightAssert(!HasValue(), "Result error read from a value");
            return std::get<1>(m_state);
        }

        T& operator*() { return GetValue(); }
        const T& operator*() const { return GetValue(); }

        decltype(auto) operator->()
        {
            if constexpr (PointerLike<T>)
            {
                return (GetValue());
            }
            else
            {
                return &GetValue();
            }
        }

        decltype(auto) operator->() const
        {
            if constexpr (PointerLike<T>)
            {
                return (GetValue());
            }
            else
            {
                return &GetValue();
            }
        }

    private:
        std::variant<T, E> m_state;
    };

    template <typename E>
    class [[nodiscard]] Result<void, E>
    {
    public:
        Result() = default;
        Result(E error) : m_error(std::move(error)) {}

        bool HasValue() const { return !m_error.has_value(); }
        explicit operator bool() const { return HasValue(); }

        const E& GetError() const
        {
            BrightAssert(!HasValue(), "Result error read from a value");
            return *m_error;
        }

    private:
        std::optional<E> m_error;
    };
}
