#include "ArenaGameMode.h"

void ArenaGameMode::OnGameUpdate(float deltaTime)
{
    m_timeRemaining = m_timeRemaining > deltaTime ? m_timeRemaining - deltaTime : 0.0f;
    if (m_restartRequested)
    {
        m_restartRequested = false;
        m_timeRemaining = m_roundLength;
    }
}
