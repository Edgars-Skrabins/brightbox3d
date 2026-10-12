#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Component;
    class Level;
    class LevelObjectInstance;

    class LevelSubsystem
    {
    public:
        explicit LevelSubsystem(Level& level) : m_level(level) {}
        virtual ~LevelSubsystem() = default;

        virtual void OnBeginPlay() {}
        virtual void OnEndPlay() {}
        virtual void OnPreFixedUpdate(float fixedDeltaTime) {}
        virtual void OnFixedUpdate(float fixedDeltaTime) {}
        virtual void OnUpdate(float deltaTime) {}
        virtual void OnLateUpdate(float deltaTime) {}
        virtual void OnComponentAdded(Component& component) {}
        virtual void OnComponentRemoved(Component& component) {}
        virtual void OnObjectTransformChanged(LevelObjectInstance& object) {}

        Level& GetLevel() const { return m_level; }

    private:
        Level& m_level;
    };

    class LevelSubsystemRegistry
    {
    public:
        using Factory = std::function<std::unique_ptr<LevelSubsystem>(Level&)>;

        static void Register(const std::string& name, Factory factory);
        static void Unregister(const std::string& name);
        static std::vector<std::pair<std::string, Factory>> GetAll();
    };
}
