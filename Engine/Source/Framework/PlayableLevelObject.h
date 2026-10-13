#pragma once

#include "Level/Categories.h"
#include "Level/LevelObject.h"

namespace BrightBox3D::Framework
{
    class Camera;
    class PlayerController;

    BrightClass(.category = Categories::Gameplay, .icon = Icon::User)
    class PlayableLevelObject : public Level::LevelObject
    {
        BrightBody()
    public:
        PlayerController* GetController() const { return m_controller; }
        Camera* FindCamera() const;

        virtual void OnPossessed(PlayerController& controller) {}
        virtual void OnUnpossessed() {}

        void CarryReferences(Reflection::ReferenceCarrier& carrier) override;

    private:
        friend class PlayerController;

        PlayerController* m_controller = nullptr;
    };
}
