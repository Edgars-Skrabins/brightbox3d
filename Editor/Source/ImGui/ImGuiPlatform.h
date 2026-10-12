#pragma once

#include "Core/Event.h"
#include "Platform/Window.h"

namespace BrightBox3D::Editor
{
    class ImGuiPlatform
    {
    public:
        explicit ImGuiPlatform(Platform::Window& window);
        ~ImGuiPlatform();

        void NewFrame(float deltaTime);

    private:
        Platform::Window& m_window;
        Core::EventHandle m_keyHandle = 0;
        Core::EventHandle m_buttonHandle = 0;
        Core::EventHandle m_moveHandle = 0;
        Core::EventHandle m_scrollHandle = 0;
        Core::EventHandle m_characterHandle = 0;
        Core::EventHandle m_focusHandle = 0;
    };
}
