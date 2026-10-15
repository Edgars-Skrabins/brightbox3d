#pragma once

#include "Reflection/EngineObject.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class LevelObjectComponent;
    class Level;
    class LevelObject;

    BrightClass(.abstract = true)
    class LevelSubsystem : public Reflection::EngineObject
    {
        BrightBody()
    public:
        explicit LevelSubsystem(Level& level) : m_level(level) {}

        virtual void OnPreFixedUpdate(float fixedDeltaTime) {}
        virtual void OnComponentAdded(LevelObjectComponent& component) {}
        virtual void OnComponentRemoved(LevelObjectComponent& component) {}
        virtual void OnObjectTransformChanged(LevelObject& object) {}

        Level* GetLevel() const override { return &m_level; }

    private:
        Level& m_level;
    };

    class LevelSubsystemRegistry
    {
    public:
        using Factory = std::function<std::unique_ptr<LevelSubsystem>(Level&)>;

        template <typename T>
        static void Register()
        {
            Register(T::StaticType(), [](Level& level) { return std::make_unique<T>(level); });
        }

        static void Register(const Reflection::TypeInfo& type, Factory factory);
        static void Unregister(const Reflection::TypeInfo& type);
        static std::vector<std::pair<std::string, Factory>> GetAll();
    };
}
