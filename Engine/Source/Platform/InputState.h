#pragma once

#include "Math/Vector2.h"
#include "Platform/Key.h"

#include <array>
#include <cstdint>
#include <string>

namespace BrightBox3D::Platform
{
    class InputState
    {
    public:
        void BeginFrame();
        void Reset();

        void SetKey(Key key, bool down);
        void SetMouseButton(MouseButton button, bool down);
        void SetMousePosition(const Math::Vector2& position);
        void AddScroll(const Math::Vector2& scroll);
        void AddText(uint32_t codepoint);

        bool IsKeyDown(Key key) const;
        bool WasKeyPressed(Key key) const;
        bool WasKeyReleased(Key key) const;
        bool IsMouseButtonDown(MouseButton button) const;
        bool WasMouseButtonPressed(MouseButton button) const;
        bool WasMouseButtonReleased(MouseButton button) const;
        Math::Vector2 GetMousePosition() const { return m_mousePosition; }
        Math::Vector2 GetMouseDelta() const { return m_mouseDelta; }
        Math::Vector2 GetScrollDelta() const { return m_scrollDelta; }
        const std::u32string& GetTextInput() const { return m_text; }

    private:
        std::array<bool, KeyCount> m_keys{};
        std::array<bool, KeyCount> m_keysSnapshot{};
        std::array<bool, KeyCount> m_previousKeys{};
        std::array<bool, MouseButtonCount> m_buttons{};
        std::array<bool, MouseButtonCount> m_buttonsSnapshot{};
        std::array<bool, MouseButtonCount> m_previousButtons{};
        Math::Vector2 m_mousePosition;
        Math::Vector2 m_previousMousePosition;
        Math::Vector2 m_mouseDelta;
        Math::Vector2 m_pendingScroll;
        Math::Vector2 m_scrollDelta;
        std::u32string m_pendingText;
        std::u32string m_text;
        bool m_hasMousePosition = false;
    };
}
