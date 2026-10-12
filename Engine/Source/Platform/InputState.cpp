#include "Platform/InputState.h"

namespace BrightBox3D::Platform
{
    namespace
    {
        bool ValidKey(Key key)
        {
            int index = static_cast<int>(key);
            return index >= 0 && index < KeyCount;
        }

        bool ValidButton(MouseButton button)
        {
            int index = static_cast<int>(button);
            return index >= 0 && index < MouseButtonCount;
        }
    }

    void InputState::BeginFrame()
    {
        m_previousKeys = m_keysSnapshot;
        m_keysSnapshot = m_keys;
        m_previousButtons = m_buttonsSnapshot;
        m_buttonsSnapshot = m_buttons;
        m_mouseDelta = m_hasMousePosition ? m_mousePosition - m_previousMousePosition : Math::Vector2();
        m_previousMousePosition = m_mousePosition;
        m_scrollDelta = m_pendingScroll;
        m_pendingScroll = {};
        m_text.swap(m_pendingText);
        m_pendingText.clear();
    }

    void InputState::Reset()
    {
        m_keys.fill(false);
        m_keysSnapshot.fill(false);
        m_previousKeys.fill(false);
        m_buttons.fill(false);
        m_buttonsSnapshot.fill(false);
        m_previousButtons.fill(false);
        m_mouseDelta = {};
        m_scrollDelta = {};
        m_pendingScroll = {};
        m_text.clear();
        m_pendingText.clear();
    }

    void InputState::SetKey(Key key, bool down)
    {
        if (ValidKey(key))
        {
            m_keys[static_cast<size_t>(key)] = down;
        }
    }

    void InputState::SetMouseButton(MouseButton button, bool down)
    {
        if (ValidButton(button))
        {
            m_buttons[static_cast<size_t>(button)] = down;
        }
    }

    void InputState::SetMousePosition(const Math::Vector2& position)
    {
        if (!m_hasMousePosition)
        {
            m_previousMousePosition = position;
            m_hasMousePosition = true;
        }
        m_mousePosition = position;
    }

    void InputState::AddScroll(const Math::Vector2& scroll)
    {
        m_pendingScroll += scroll;
    }

    void InputState::AddText(uint32_t codepoint)
    {
        m_pendingText.push_back(static_cast<char32_t>(codepoint));
    }

    bool InputState::IsKeyDown(Key key) const
    {
        return ValidKey(key) && m_keysSnapshot[static_cast<size_t>(key)];
    }

    bool InputState::WasKeyPressed(Key key) const
    {
        return ValidKey(key) && m_keysSnapshot[static_cast<size_t>(key)] && !m_previousKeys[static_cast<size_t>(key)];
    }

    bool InputState::WasKeyReleased(Key key) const
    {
        return ValidKey(key) && !m_keysSnapshot[static_cast<size_t>(key)] && m_previousKeys[static_cast<size_t>(key)];
    }

    bool InputState::IsMouseButtonDown(MouseButton button) const
    {
        return ValidButton(button) && m_buttonsSnapshot[static_cast<size_t>(button)];
    }

    bool InputState::WasMouseButtonPressed(MouseButton button) const
    {
        return ValidButton(button) && m_buttonsSnapshot[static_cast<size_t>(button)] && !m_previousButtons[static_cast<size_t>(button)];
    }

    bool InputState::WasMouseButtonReleased(MouseButton button) const
    {
        return ValidButton(button) && !m_buttonsSnapshot[static_cast<size_t>(button)] && m_previousButtons[static_cast<size_t>(button)];
    }
}
