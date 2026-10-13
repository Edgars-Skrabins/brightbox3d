#pragma once

#include "Framework/Categories.h"
#include "Level/Categories.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Platform/Key.h"
#include "Reflection/EngineObject.h"

namespace BrightBox3D::Framework
{
    class Camera;
    class PlayableLevelObject;

    BrightClass(.category = Categories::Gameplay)
    class PlayerController : public Reflection::EngineObject
    {
        BrightBody()
    public:
        void OnGameStart() override;
        void OnGameUpdateLate(float deltaTime) override;
        void OnDestroy() override;
        void CarryReferences(Reflection::ReferenceCarrier& carrier) override;

        int32_t GetPlayerIndex() const { return m_playerIndex; }

        void Possess(PlayableLevelObject& playable);
        void UnPossess();
        PlayableLevelObject* GetPlayable() const { return m_playable; }

        void SetViewTarget(Camera* camera);
        Camera* GetViewTarget() const { return m_viewTarget; }

        bool IsKeyDown(Platform::Key key) const;
        bool WasKeyPressed(Platform::Key key) const;
        Math::Vector2 GetMouseDelta() const;
        void SetCursorLocked(bool locked);

        float GetYaw() const { return m_yaw; }
        float GetPitch() const { return m_pitch; }
        void AddLookInput(float yawDegrees, float pitchDegrees);

    private:
        friend class GameModeBase;
        friend class GameSession;

        Camera* CreateViewCamera(PlayableLevelObject& playable);
        void HandlePlayableDestroyed();

        BrightProperty(.edit = Edit::Visible, .category = Categories::Player)
        int32_t m_playerIndex = 0;

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Camera)
        bool m_spawnCamera = true;

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Camera)
        Math::Vector3 m_cameraOffset{ 0.0f, 1.6f, 0.0f };

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Camera)
        bool m_cameraUsesLookRotation = true;

        BrightProperty(.edit = Edit::DefaultsOnly, .category = Categories::Input)
        bool m_lockCursorOnStart = false;

        PlayableLevelObject* m_playable = nullptr;
        Camera* m_viewTarget = nullptr;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
    };
}
