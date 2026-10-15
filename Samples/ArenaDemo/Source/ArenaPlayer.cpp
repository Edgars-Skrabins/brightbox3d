#include "ArenaPlayer.h"

#include "Framework/PlayerController.h"
#include "Platform/Key.h"

using BrightBox3D::Platform::Key;

void ArenaPlayer::OnGameStart()
{
    m_health = 100;
}

void ArenaPlayer::OnGameUpdate(float deltaTime)
{
    BrightBox3D::Framework::PlayerController* controller = GetController();
    if (!controller)
    {
        return;
    }
    BrightBox3D::Math::Vector2 mouse = controller->GetMouseDelta();
    controller->AddLookInput(mouse.x * m_lookSensitivity, mouse.y * m_lookSensitivity);
    BrightBox3D::Math::Vector3 input;
    if (controller->IsKeyDown(Key::W)) input.z += 1.0f;
    if (controller->IsKeyDown(Key::S)) input.z -= 1.0f;
    if (controller->IsKeyDown(Key::D)) input.x += 1.0f;
    if (controller->IsKeyDown(Key::A)) input.x -= 1.0f;
    GetTransform().SetRotation(BrightBox3D::Math::Quaternion::Euler(0.0f, controller->GetYaw(), 0.0f));
    GetTransform().Translate(input.Normalized() * m_moveSpeed * deltaTime);
}
