#include "Framework/GameStateBase.h"

#include "Framework/GameModeBase.h"
#include "Level/Level.h"

namespace BrightBox3D::Framework
{
    void GameStateBase::CarryReferences(Reflection::ReferenceCarrier& carrier)
    {
        carrier.Carry(m_gameMode);
    }

    GameStateBase* GameStateBase::Get(const Level::Level& level)
    {
        return level.FindEngineObjectOfType<GameStateBase>();
    }
}
