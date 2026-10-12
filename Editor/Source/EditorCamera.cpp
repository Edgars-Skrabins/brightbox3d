#include "EditorCamera.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    Math::Quaternion EditorCamera::GetRotation() const
    {
        return Math::Quaternion::Euler(m_pitch, m_yaw, 0.0f);
    }

    Math::Vector3 EditorCamera::GetForward() const
    {
        return GetRotation() * Math::Vector3::Forward();
    }

    Math::Matrix4 EditorCamera::GetViewMatrix() const
    {
        Math::Quaternion rotation = GetRotation();
        return Math::Matrix4::LookAtLH(m_position, m_position + rotation * Math::Vector3::Forward(), rotation * Math::Vector3::Up());
    }

    Math::Matrix4 EditorCamera::GetProjectionMatrix(float aspect) const
    {
        return Math::Matrix4::PerspectiveFovLH(m_verticalFov * Math::DegreesToRadians, aspect, m_nearPlane, m_farPlane);
    }

    void EditorCamera::Focus(const Math::Vector3& target, float radius)
    {
        float distance = std::max(radius, 0.5f) * 2.5f / std::tan(m_verticalFov * Math::DegreesToRadians * 0.5f) * 0.5f;
        m_focusFrom = m_position;
        m_focusTo = target - GetForward() * distance;
        m_orbitPivot = target;
        m_focusTimer = 0.25f;
    }

    void EditorCamera::Update(float deltaTime, bool hovered, bool focused)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (m_focusTimer > 0.0f)
        {
            m_focusTimer = std::max(0.0f, m_focusTimer - deltaTime);
            float t = 1.0f - m_focusTimer / 0.25f;
            t = t * t * (3.0f - 2.0f * t);
            m_position = Math::Vector3::Lerp(m_focusFrom, m_focusTo, t);
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            m_isFlying = true;
        }
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        {
            m_isPanning = true;
        }
        if (hovered && io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_isOrbiting = true;
            m_orbitPivot = m_position + GetForward() * std::max((m_orbitPivot - m_position).Length(), 5.0f);
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            m_isFlying = false;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            m_isPanning = false;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_isOrbiting = false;
        }

        ImVec2 delta = io.MouseDelta;
        Math::Quaternion rotation = GetRotation();
        if (m_isFlying)
        {
            m_pitch = Math::Clamp(m_pitch + delta.y * 0.2f, -89.0f, 89.0f);
            m_yaw += delta.x * 0.2f;
            Math::Vector3 move;
            if (ImGui::IsKeyDown(ImGuiKey_W))
            {
                move += Math::Vector3::Forward();
            }
            if (ImGui::IsKeyDown(ImGuiKey_S))
            {
                move -= Math::Vector3::Forward();
            }
            if (ImGui::IsKeyDown(ImGuiKey_D))
            {
                move += Math::Vector3::Right();
            }
            if (ImGui::IsKeyDown(ImGuiKey_A))
            {
                move -= Math::Vector3::Right();
            }
            if (ImGui::IsKeyDown(ImGuiKey_E))
            {
                move += Math::Vector3::Up();
            }
            if (ImGui::IsKeyDown(ImGuiKey_Q))
            {
                move -= Math::Vector3::Up();
            }
            if (io.MouseWheel != 0.0f)
            {
                m_speed = Math::Clamp(m_speed * (io.MouseWheel > 0.0f ? 1.2f : 1.0f / 1.2f), 0.5f, 500.0f);
            }
            float speed = m_speed * (io.KeyShift ? 3.0f : 1.0f);
            m_position += GetRotation() * move.Normalized() * speed * deltaTime;
            m_focusTimer = 0.0f;
        }
        else if (m_isPanning)
        {
            float scale = 0.01f * std::max(1.0f, m_speed * 0.2f);
            m_position += rotation * Math::Vector3(-delta.x * scale, delta.y * scale, 0.0f);
            m_focusTimer = 0.0f;
        }
        else if (m_isOrbiting)
        {
            float distance = (m_orbitPivot - m_position).Length();
            m_pitch = Math::Clamp(m_pitch + delta.y * 0.25f, -89.0f, 89.0f);
            m_yaw += delta.x * 0.25f;
            m_position = m_orbitPivot - GetForward() * distance;
            m_focusTimer = 0.0f;
        }
        else if (hovered && io.MouseWheel != 0.0f)
        {
            m_position += rotation * Math::Vector3::Forward() * io.MouseWheel * std::max(0.5f, m_speed * 0.25f);
            m_focusTimer = 0.0f;
        }
    }
}
