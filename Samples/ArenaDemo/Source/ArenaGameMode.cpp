#include "ArenaGameMode.h"

#include "ArenaGameState.h"

ArenaGameMode::ArenaGameMode()
{
    m_gameStateClass.Set<ArenaGameState>();
}

void ArenaGameMode::OnGameUpdate(float deltaTime)
{
    m_timeRemaining = m_timeRemaining > deltaTime ? m_timeRemaining - deltaTime : 0.0f;
    if (m_restartRequested)
    {
        m_restartRequested = false;
        m_timeRemaining = m_roundLength;
        if (auto* state = BrightBox3D::Reflection::Cast<ArenaGameState>(GetGameState()))
        {
            state->ResetScore();
        }
    }
}
