#include "Framework/PlayerController.h"

#include "Framework/Camera.h"
#include "Level/Level.h"
#include "Level/LevelSerializer.h"
#include "Platform/Input.h"

namespace BrightBox3D::Framework
{
    void PlayerController::OnGameStart()
    {
        Level::Level* level = GetLevel();
        m_yaw = GetTransform().GetEulerAngles().y;
        if (m_spawnCamera && level && !GetCamera())
        {
            Level::LevelObjectInstance* spawned = nullptr;
            if (m_cameraLevelObject.IsSet())
            {
                spawned = Level::LevelSerializer::Instantiate(*level, m_cameraLevelObject.guid, GetOwner()->GetCollection(), nullptr);
            }
            if (!spawned || !spawned->IsA<Camera>())
            {
                if (spawned)
                {
                    level->DestroyObject(spawned);
                }
                spawned = level->CreateObject<Camera>(GetOwner()->GetCollection());
            }
            spawned->SetName(GetOwner()->GetName() + " Camera");
            spawned->SetOwnerId(GetOwner()->GetId());
            SetCamera(static_cast<Camera*>(spawned));
            OnGameUpdateLate(0.0f);
        }
        if (m_lockCursorOnStart && m_playerIndex == 0)
        {
            SetCursorLocked(true);
        }
    }

    void PlayerController::OnGameUpdateLate(float deltaTime)
    {
        Camera* camera = GetCamera();
        if (!camera || !m_cameraFollow)
        {
            return;
        }
        const Level::Transform& transform = GetTransform();
        camera->GetTransform().SetPosition(transform.GetPosition() + transform.GetRotation() * m_cameraOffset);
        if (m_cameraUsesLookRotation)
        {
            camera->GetTransform().SetRotation(Math::Quaternion::Euler(m_pitch, m_yaw, 0.0f));
        }
        else
        {
            camera->GetTransform().SetRotation(transform.GetRotation());
        }
    }

    void PlayerController::OnDestroy()
    {
        if (Camera* camera = GetCamera())
        {
            camera->Destroy();
        }
        if (m_playerIndex == 0 && Platform::Input::IsCursorLocked())
        {
            SetCursorLocked(false);
        }
    }

    Camera* PlayerController::GetCamera() const
    {
        Level::Level* level = GetLevel();
        if (!level || !m_camera.IsValid())
        {
            return nullptr;
        }
        return Reflection::Cast<Camera>(level->FindObject(m_camera));
    }

    void PlayerController::SetCamera(Camera* camera)
    {
        m_camera = camera ? camera->GetId() : Core::Guid{};
        if (camera)
        {
            camera->SetPriority(std::max(camera->GetPriority(), 100 - m_playerIndex));
        }
    }

    bool PlayerController::IsKeyDown(Platform::Key key) const
    {
        return m_playerIndex == 0 && Platform::Input::IsKeyDown(key);
    }

    bool PlayerController::WasKeyPressed(Platform::Key key) const
    {
        return m_playerIndex == 0 && Platform::Input::WasKeyPressed(key);
    }

    Math::Vector2 PlayerController::GetMouseDelta() const
    {
        return m_playerIndex == 0 ? Platform::Input::GetMouseDelta() : Math::Vector2();
    }

    void PlayerController::SetCursorLocked(bool locked)
    {
        if (m_playerIndex == 0)
        {
            Platform::Input::SetCursorLocked(locked);
        }
    }

    void PlayerController::AddLookInput(float yawDegrees, float pitchDegrees)
    {
        m_yaw += yawDegrees;
        m_pitch = Math::Clamp(m_pitch + pitchDegrees, -89.0f, 89.0f);
    }
}
