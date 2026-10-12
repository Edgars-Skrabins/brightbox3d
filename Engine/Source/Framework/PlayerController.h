#pragma once

#include "Assets/AssetDatabase.h"
#include "Level/Component.h"
#include "Level/LevelAssets.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Platform/Key.h"

namespace BrightBox3D::Framework
{
    class Camera;

    BrightClass(Category = "Gameplay")
    class PlayerController : public Level::Component
    {
        BrightBody()
    public:
        void OnGameStart() override;
        void OnGameUpdateLate(float deltaTime) override;
        void OnDestroy() override;

        int32_t GetPlayerIndex() const { return m_playerIndex; }
        void SetPlayerIndex(int32_t index) { m_playerIndex = index; }
        Camera* GetCamera() const;
        void SetCamera(Camera* camera);

        bool IsKeyDown(Platform::Key key) const;
        bool WasKeyPressed(Platform::Key key) const;
        Math::Vector2 GetMouseDelta() const;
        void SetCursorLocked(bool locked);

        float GetYaw() const { return m_yaw; }
        float GetPitch() const { return m_pitch; }
        void AddLookInput(float yawDegrees, float pitchDegrees);

    private:
        BrightProperty(VisibleAnywhere, Category = "Player")
        int32_t m_playerIndex = 0;

        BrightProperty(EditAnywhere, Category = "Camera")
        bool m_spawnCamera = true;

        BrightProperty(EditAnywhere, Category = "Camera")
        Assets::AssetReference<Level::LevelObjectAsset> m_cameraLevelObject;

        BrightProperty(EditAnywhere, Category = "Camera")
        bool m_cameraFollow = true;

        BrightProperty(EditAnywhere, Category = "Camera")
        Math::Vector3 m_cameraOffset{ 0.0f, 1.6f, 0.0f };

        BrightProperty(EditAnywhere, Category = "Camera")
        bool m_cameraUsesLookRotation = true;

        BrightProperty(EditAnywhere, Category = "Input")
        bool m_lockCursorOnStart = false;

        Core::Guid m_camera;
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
    };
}
