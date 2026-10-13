#include "Runtime/FrameClock.h"

#include "Platform/Window.h"

#include <algorithm>

namespace BrightBox3D::Runtime
{
    FrameClock::FrameClock() : m_previous(Platform::Window::GetTime())
    {
    }

    float FrameClock::Tick()
    {
        double now = Platform::Window::GetTime();
        double deltaTime = std::min(now - m_previous, MaxFrameDeltaTime);
        m_previous = now;
        return static_cast<float>(deltaTime);
    }

    void FrameClock::WaitWhileMinimized()
    {
        Platform::Window::WaitEvents(MinimizedWaitTime);
    }
}
