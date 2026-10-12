#pragma once

#include "Math/Matrix4.h"

namespace BrightBox3D::Editor
{
    class EditorCamera
    {
    public:
        void Update(float deltaTime, bool hovered, bool focused);
        void Focus(const Math::Vector3& target, float radius);

        Math::Matrix4 GetViewMatrix() const;
        Math::Matrix4 GetProjectionMatrix(float aspect) const;
        const Math::Vector3& GetPosition() const { return m_position; }
        Math::Quaternion GetRotation() const;
        Math::Vector3 GetForward() const;
        float GetVerticalFov() const { return m_verticalFov; }
        float GetNearPlane() const { return m_nearPlane; }
        float GetFarPlane() const { return m_farPlane; }
        float GetSpeed() const { return m_speed; }
        void SetSpeed(float speed) { m_speed = speed; }
        bool IsFlying() const { return m_isFlying; }
        void SetPosition(const Math::Vector3& position) { m_position = position; }
        void SetAngles(float pitch, float yaw)
        {
            m_pitch = pitch;
            m_yaw = yaw;
        }
        float GetPitch() const { return m_pitch; }
        float GetYaw() const { return m_yaw; }

    private:
        Math::Vector3 m_position{ 0.0f, 3.0f, -8.0f };
        float m_pitch = 15.0f;
        float m_yaw = 0.0f;
        float m_speed = 6.0f;
        float m_verticalFov = 60.0f;
        float m_nearPlane = 0.05f;
        float m_farPlane = 2000.0f;
        bool m_isFlying = false;
        bool m_isPanning = false;
        bool m_isOrbiting = false;
        Math::Vector3 m_orbitPivot;
        float m_focusTimer = 0.0f;
        Math::Vector3 m_focusFrom;
        Math::Vector3 m_focusTo;
    };
}
