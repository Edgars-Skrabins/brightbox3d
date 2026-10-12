#pragma once

#include <chrono>

namespace BrightBox3D::Core
{
    class Stopwatch
    {
    public:
        Stopwatch() : m_start(std::chrono::steady_clock::now()) {}

        void Restart() { m_start = std::chrono::steady_clock::now(); }

        double ElapsedSeconds() const
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
        }

        double ElapsedMilliseconds() const { return ElapsedSeconds() * 1000.0; }

    private:
        std::chrono::steady_clock::time_point m_start;
    };
}
