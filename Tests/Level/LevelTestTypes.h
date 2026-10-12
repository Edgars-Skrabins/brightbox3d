#pragma once

#include "Level/LevelObjectInstance.h"

#include <string>
#include <vector>

namespace BrightBox3D::Tests
{
    std::vector<std::string>& LifecycleLog();

    BrightClass()
    class RecordingComponent : public Level::Component
    {
        BrightBody()
    public:
        void OnInitialize() override { Record("Initialize"); }
        void OnGameLoad() override { Record("Load"); }
        void OnGameStart() override { Record("Start"); }
        void OnGameUpdate(float deltaTime) override { ++m_updates; }
        void OnGameUpdateFixed(float fixedDeltaTime) override { ++m_fixedUpdates; }
        void OnGameUpdateLate(float deltaTime) override { ++m_lateUpdates; }
        void OnEnable() override { Record("Enable"); }
        void OnDisable() override { Record("Disable"); }
        void OnGameEnd() override { Record("End"); }
        void OnDestroy() override { Record("Destroy"); }

        int m_updates = 0;
        int m_fixedUpdates = 0;
        int m_lateUpdates = 0;

        BrightProperty(EditAnywhere)
        std::string m_label = "component";

        BrightProperty(EditAnywhere)
        float m_power = 1.0f;

    private:
        void Record(const std::string& event) { LifecycleLog().push_back(m_label + ":" + event); }
    };

    BrightClass()
    class TestPlayer : public Level::LevelObjectInstance
    {
        BrightBody()
    public:
        void OnGameLoad() override { LifecycleLog().push_back("Player:Load"); }
        void OnGameStart() override { LifecycleLog().push_back("Player:Start"); }
        void OnGameUpdate(float deltaTime) override { m_elapsed += deltaTime; }
        void OnGameEnd() override { LifecycleLog().push_back("Player:End"); }
        void OnDestroy() override { LifecycleLog().push_back("Player:Destroy"); }

        float m_elapsed = 0.0f;

        BrightProperty(EditDefaultsOnly, Category = "Movement")
        float m_moveSpeed = 600.0f;

        BrightProperty(EditAnywhere)
        int m_lives = 3;
    };
}
