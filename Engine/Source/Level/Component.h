#pragma once

#include "Core/Guid.h"
#include "Math/Vector3.h"
#include "Reflection/Object.h"

namespace BrightBox3D::Level
{
    class Level;
    class LevelObjectInstance;
    class LevelSerializerAccess;

    struct CollisionInfo
    {
        LevelObjectInstance* other = nullptr;
        Math::Vector3 point;
        Math::Vector3 normal;
        Math::Vector3 relativeVelocity;
        bool isTrigger = false;
    };
    class ObjectReplacer;
    class Transform;

    BrightClass(Abstract)
    class Component : public Reflection::Object
    {
        BrightBody()
    public:
        virtual void OnInitialize() {}
        virtual void OnGameLoad() {}
        virtual void OnGameStart() {}
        virtual void OnGameUpdate(float deltaTime) {}
        virtual void OnGameUpdateFixed(float fixedDeltaTime) {}
        virtual void OnGameUpdateLate(float deltaTime) {}
        virtual void OnEnable() {}
        virtual void OnDisable() {}
        virtual void OnGameEnd() {}
        virtual void OnDestroy() {}
        virtual void OnCollisionEnter(const CollisionInfo& collision) {}
        virtual void OnCollisionExit(const CollisionInfo& collision) {}
        virtual void OnTriggerEnter(const CollisionInfo& collision) {}
        virtual void OnTriggerExit(const CollisionInfo& collision) {}

        LevelObjectInstance* GetOwner() const { return m_owner; }
        Transform& GetTransform() const;
        Level* GetLevel() const;
        bool IsEnabled() const { return m_isEnabled; }
        void SetEnabled(bool enabled);
        bool IsActiveAndEnabled() const;
        const Core::Guid& GetLocalId() const { return m_localId; }
        void SetLocalId(const Core::Guid& localId) { m_localId = localId; }
        bool HasStarted() const { return m_hasStarted; }
        bool IsPendingDestroy() const { return m_isPendingDestroy; }

    private:
        friend class LevelObjectInstance;
        friend class Level;
        friend class LevelSerializerAccess;
        friend class ObjectReplacer;

        LevelObjectInstance* m_owner = nullptr;
        Core::Guid m_localId;
        bool m_isEnabled = true;
        bool m_hasLoaded = false;
        bool m_hasStarted = false;
        bool m_isEnabledState = false;
        bool m_isPendingDestroy = false;
    };
}
