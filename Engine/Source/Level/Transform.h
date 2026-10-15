#pragma once

#include "Level/Categories.h"
#include "Level/LevelObjectComponent.h"
#include "Math/Matrix4.h"

namespace BrightBox3D::Level
{
    BrightClass()
    class Transform : public LevelObjectComponent
    {
        BrightBody()
    public:
        const Math::Vector3& GetLocalPosition() const { return m_localPosition; }
        const Math::Quaternion& GetLocalRotation() const { return m_localRotation; }
        const Math::Vector3& GetLocalScale() const { return m_localScale; }
        Math::Vector3 GetLocalEulerAngles() const { return m_localRotation.ToEuler(); }
        void SetLocalPosition(const Math::Vector3& position);
        void SetLocalRotation(const Math::Quaternion& rotation);
        void SetLocalScale(const Math::Vector3& scale);
        void SetLocalEulerAngles(const Math::Vector3& degrees);

        Math::Vector3 GetPosition() const;
        Math::Quaternion GetRotation() const;
        Math::Vector3 GetEulerAngles() const { return GetRotation().ToEuler(); }
        Math::Vector3 GetLossyScale() const;
        void SetPosition(const Math::Vector3& position);
        void SetRotation(const Math::Quaternion& rotation);
        void SetEulerAngles(const Math::Vector3& degrees);
        void SetPositionAndRotation(const Math::Vector3& position, const Math::Quaternion& rotation);

        Math::Vector3 GetForward() const;
        Math::Vector3 GetRight() const;
        Math::Vector3 GetUp() const;

        void Translate(const Math::Vector3& offset, bool worldSpace = false);
        void Rotate(const Math::Vector3& eulerDegrees, bool worldSpace = false);
        void RotateAround(const Math::Vector3& point, const Math::Vector3& axis, float degrees);
        void LookAt(const Math::Vector3& target, const Math::Vector3& up = Math::Vector3::Up());

        Math::Matrix4 GetLocalMatrix() const;
        const Math::Matrix4& GetWorldMatrix() const;
        Math::Matrix4 GetWorldToLocalMatrix() const;
        Math::Vector3 TransformPoint(const Math::Vector3& point) const;
        Math::Vector3 InverseTransformPoint(const Math::Vector3& point) const;
        Math::Vector3 TransformDirection(const Math::Vector3& direction) const;
        Math::Vector3 InverseTransformDirection(const Math::Vector3& direction) const;

        void SetWorldMatrix(const Math::Matrix4& world);
        void MarkDirty();
        void OnPropertiesChanged() override { MarkDirty(); }
        uint64_t GetVersion() const { return m_version; }

    private:
        const Transform* GetParentTransform() const;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Transform, .displayName = DisplayNames::Position)
        Math::Vector3 m_localPosition;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Transform, .displayName = DisplayNames::Rotation)
        Math::Quaternion m_localRotation;

        BrightProperty(.edit = Edit::Anywhere, .category = Categories::Transform, .displayName = DisplayNames::Scale)
        Math::Vector3 m_localScale{ 1.0f, 1.0f, 1.0f };

        mutable Math::Matrix4 m_worldMatrix;
        mutable bool m_isWorldDirty = true;
        uint64_t m_version = 0;
    };
}
