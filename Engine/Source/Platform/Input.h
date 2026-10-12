#pragma once

#include "Math/Vector2.h"
#include "Platform/Key.h"

namespace BrightBox3D::Platform
{
    class Window;

    class Input
    {
    public:
        static bool IsKeyDown(Key key);
        static bool WasKeyPressed(Key key);
        static bool WasKeyReleased(Key key);
        static bool IsMouseButtonDown(MouseButton button);
        static bool WasMouseButtonPressed(MouseButton button);
        static bool WasMouseButtonReleased(MouseButton button);
        static Math::Vector2 GetMousePosition();
        static Math::Vector2 GetMouseDelta();
        static Math::Vector2 GetScrollDelta();
        static float GetAxis(Key negative, Key positive);
        static void SetCursorLocked(bool locked);
        static bool IsCursorLocked();
        static bool IsWindowFocused();

        static void SetWindow(Window* window);
        static Window* GetWindow();
        static void SetEnabled(bool enabled);
    };
}
