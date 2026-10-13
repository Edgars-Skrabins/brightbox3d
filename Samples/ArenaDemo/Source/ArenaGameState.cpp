#include "ArenaGameState.h"

void ArenaGameState::OnGameUpdate(float deltaTime)
{
    m_elapsed += deltaTime;
    while (m_elapsed >= 1.0f)
    {
        m_elapsed -= 1.0f;
        ++m_score;
    }
}

void ArenaGameState::ResetScore()
{
    m_score = 0;
    m_elapsed = 0.0f;
}
