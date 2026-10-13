#pragma once

#include "Level/Categories.h"
#include "Reflection/EngineObject.h"

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Framework
{
    class GameModeBase;

    BrightClass(.category = Categories::Gameplay)
    class GameStateBase : public Reflection::EngineObject
    {
        BrightBody()
    public:
        GameModeBase* GetGameMode() const { return m_gameMode; }
        void CarryReferences(Reflection::ReferenceCarrier& carrier) override;

        static GameStateBase* Get(const Level::Level& level);

    private:
        friend class GameModeBase;

        GameModeBase* m_gameMode = nullptr;
    };
}
