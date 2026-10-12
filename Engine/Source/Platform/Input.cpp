#include "Platform/Input.h"

#include "Platform/Window.h"

namespace BrightBox3D::Platform
{
    namespace
    {
        Window* s_window = nullptr;
        bool s_enabled = true;

        const InputState* State()
        {
            return s_window && s_enabled ? &s_window->GetInput() : nullptr;
        }
    }

    bool Input::IsKeyDown(Key key)
    {
        const InputState* state = State();
        return state && state->IsKeyDown(key);
    }

    bool Input::WasKeyPressed(Key key)
    {
        const InputState* state = State();
        return state && state->WasKeyPressed(key);
    }

    bool Input::WasKeyReleased(Key key)
    {
        const InputState* state = State();
        return state && state->WasKeyReleased(key);
    }

    bool Input::IsMouseButtonDown(MouseButton button)
    {
        const InputState* state = State();
        return state && state->IsMouseButtonDown(button);
    }

    bool Input::WasMouseButtonPressed(MouseButton button)
    {
        const InputState* state = State();
        return state && state->WasMouseButtonPressed(button);
    }

    bool Input::WasMouseButtonReleased(MouseButton button)
    {
        const InputState* state = State();
        return state && state->WasMouseButtonReleased(button);
    }

    Math::Vector2 Input::GetMousePosition()
    {
        const InputState* state = State();
        return state ? state->GetMousePosition() : Math::Vector2();
    }

    Math::Vector2 Input::GetMouseDelta()
    {
        const InputState* state = State();
        return state ? state->GetMouseDelta() : Math::Vector2();
    }

    Math::Vector2 Input::GetScrollDelta()
    {
        const InputState* state = State();
        return state ? state->GetScrollDelta() : Math::Vector2();
    }

    float Input::GetAxis(Key negative, Key positive)
    {
        return (IsKeyDown(positive) ? 1.0f : 0.0f) - (IsKeyDown(negative) ? 1.0f : 0.0f);
    }

    void Input::SetCursorLocked(bool locked)
    {
        if (s_window)
        {
            s_window->SetCursorMode(locked ? CursorMode::Locked : CursorMode::Normal);
        }
    }

    bool Input::IsCursorLocked()
    {
        return s_window && s_window->GetCursorMode() == CursorMode::Locked;
    }

    bool Input::IsWindowFocused()
    {
        return s_window && s_window->IsFocused();
    }

    void Input::SetWindow(Window* window)
    {
        s_window = window;
    }

    Window* Input::GetWindow()
    {
        return s_window;
    }

    void Input::SetEnabled(bool enabled)
    {
        s_enabled = enabled;
    }
}
