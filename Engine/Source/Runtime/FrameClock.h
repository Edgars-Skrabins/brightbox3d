#pragma once

namespace BrightBox3D::Runtime
{
    class FrameClock
    {
    public:
        static constexpr double MaxFrameDeltaTime = 0.1;
        static constexpr double MinimizedWaitTime = 0.1;

        FrameClock();

        float Tick();
        static void WaitWhileMinimized();

    private:
        double m_previous = 0.0;
    };
}
