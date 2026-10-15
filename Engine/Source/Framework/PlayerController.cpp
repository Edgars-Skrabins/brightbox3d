#include "Framework/PlayerController.h"

#include "Core/EngineText.h"
#include "Framework/Camera.h"
#include "Framework/GameModeBase.h"
#include "Framework/PlayableLevelObject.h"
#include "Level/Level.h"
#include "Platform/Input.h"

namespace BrightBox3D::Framework
{
    namespace
    {
        constexpr float MaxLookPitch = 89.0f;
        constexpr int32_t ViewTargetPriority = 100;
    }

    void PlayerController::OnGameStart()
    {
        if (m_lockCursorOnStart)
        {
            SetCursorLocked(true);
        }
    }

    void PlayerController::OnGameUpdateLate(float deltaTime)
    {
        if (m_viewTarget && m_cameraUsesLookRotation)
        {
            m_viewTarget->GetTransform().SetRotation(Math::Quaternion::Euler(m_pitch, m_yaw, 0.0f));
        }
    }

    void PlayerController::OnDestroy()
    {
        UnPossess();
        if (Platform::Input::IsCursorLocked())
        {
            SetCursorLocked(false);
        }
    }

    void PlayerController::CarryReferences(Reflection::ReferenceCarrier& carrier)
    {
        carrier.Carry(m_playable);
        carrier.Carry(m_viewTarget);
    }

    void PlayerController::Possess(PlayableLevelObject& playable)
    {
        if (m_playable == &playable)
        {
            return;
        }
        if (PlayerController* previous = playable.GetController())
        {
            previous->UnPossess();
        }
        UnPossess();
        m_playable = &playable;
        playable.m_controller = this;
        m_yaw = playable.GetTransform().GetEulerAngles().y;
        m_pitch = 0.0f;
        Camera* camera = playable.FindCamera();
        if (!camera && m_spawnCamera)
        {
            camera = CreateViewCamera(playable);
        }
        if (camera)
        {
            SetViewTarget(camera);
        }
        playable.OnPossessed(*this);
    }

    void PlayerController::UnPossess()
    {
        PlayableLevelObject* playable = m_playable;
        if (!playable)
        {
            return;
        }
        m_playable = nullptr;
        playable->m_controller = nullptr;
        playable->OnUnpossessed();
    }

    void PlayerController::SetViewTarget(Camera* camera)
    {
        m_viewTarget = camera;
        if (camera)
        {
            camera->SetPriority(std::max(camera->GetPriority(), ViewTargetPriority - m_playerIndex));
        }
    }

    Camera* PlayerController::CreateViewCamera(PlayableLevelObject& playable)
    {
        Level::Level* level = playable.GetLevel();
        if (!level)
        {
            return nullptr;
        }
        auto* camera = level->CreateObject<Camera>(playable.GetCollection(), &playable);
        camera->SetName(Core::Text(Core::EngineText::CameraName, playable.GetName()));
        camera->SetOwnerId(playable.GetId());
        camera->GetTransform().SetLocalPosition(m_cameraOffset);
        return camera;
    }

    void PlayerController::HandlePlayableDestroyed()
    {
        PlayableLevelObject* playable = m_playable;
        if (m_viewTarget && playable && (m_viewTarget == static_cast<Level::LevelObject*>(playable) || m_viewTarget->IsDescendantOf(playable)))
        {
            m_viewTarget = nullptr;
        }
        UnPossess();
        if (Level::Level* level = GetLevel())
        {
            if (GameModeBase* gameMode = GameModeBase::Get(*level))
            {
                gameMode->OnPlayableDestroyed(*this);
            }
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
        m_pitch = Math::Clamp(m_pitch + pitchDegrees, -MaxLookPitch, MaxLookPitch);
    }
}
