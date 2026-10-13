#pragma once

#include "Core/Guid.h"
#include "Reflection/EngineObject.h"

namespace BrightBox3D::Level
{
    class Level;
    class LevelObject;
    class Transform;

    struct ComponentState
    {
        Core::Guid localId;
        bool isEnabled = true;
    };

    BrightClass(.abstract = true)
    class LevelObjectComponent : public Reflection::EngineObject
    {
        BrightBody()
    public:
        LevelObject* GetOwner() const { return m_owner; }
        Transform& GetTransform() const;
        Level* GetLevel() const override;
        bool IsEnabled() const { return m_state.isEnabled; }
        void SetEnabled(bool enabled);
        bool IsActiveAndEnabled() const;
        const Core::Guid& GetLocalId() const { return m_state.localId; }
        void SetLocalId(const Core::Guid& localId) { m_state.localId = localId; }

    private:
        friend class LevelObject;
        friend class Level;

        LevelObject* m_owner = nullptr;
        ComponentState m_state;
    };
}
