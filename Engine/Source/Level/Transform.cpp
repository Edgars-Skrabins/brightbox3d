#include "Level/Transform.h"

#include "Level/LevelObject.h"

namespace BrightBox3D::Level
{
    const Transform* Transform::GetParentTransform() const
    {
        LevelObject* owner = GetOwner();
        if (!owner || !owner->GetParent())
        {
            return nullptr;
        }
        return &owner->GetParent()->GetTransform();
    }

    void Transform::MarkDirty()
    {
        m_isWorldDirty = true;
        ++m_version;
        if (LevelObject* owner = GetOwner())
        {
            for (LevelObject* child : owner->GetChildren())
            {
                child->GetTransform().MarkDirty();
            }
        }
    }

    void Transform::SetLocalPosition(const Math::Vector3& position)
    {
        m_localPosition = position;
        MarkDirty();
    }

    void Transform::SetLocalRotation(const Math::Quaternion& rotation)
    {
        m_localRotation = rotation.Normalized();
        MarkDirty();
    }

    void Transform::SetLocalScale(const Math::Vector3& scale)
    {
        m_localScale = scale;
        MarkDirty();
    }

    void Transform::SetLocalEulerAngles(const Math::Vector3& degrees)
    {
        SetLocalRotation(Math::Quaternion::Euler(degrees));
    }

    Math::Vector3 Transform::GetPosition() const
    {
        return GetWorldMatrix().GetTranslation();
    }

    Math::Quaternion Transform::GetRotation() const
    {
        const Transform* parent = GetParentTransform();
        return parent ? (parent->GetRotation() * m_localRotation).Normalized() : m_localRotation;
    }

    Math::Vector3 Transform::GetLossyScale() const
    {
        const Math::Matrix4& world = GetWorldMatrix();
        return { world.GetRight().Length(), world.GetUp().Length(), world.GetForward().Length() };
    }

    void Transform::SetPosition(const Math::Vector3& position)
    {
        const Transform* parent = GetParentTransform();
        SetLocalPosition(parent ? parent->InverseTransformPoint(position) : position);
    }

    void Transform::SetRotation(const Math::Quaternion& rotation)
    {
        const Transform* parent = GetParentTransform();
        SetLocalRotation(parent ? parent->GetRotation().Inverse() * rotation : rotation);
    }

    void Transform::SetEulerAngles(const Math::Vector3& degrees)
    {
        SetRotation(Math::Quaternion::Euler(degrees));
    }

    void Transform::SetPositionAndRotation(const Math::Vector3& position, const Math::Quaternion& rotation)
    {
        SetPosition(position);
        SetRotation(rotation);
    }

    Math::Vector3 Transform::GetForward() const
    {
        return GetRotation() * Math::Vector3::Forward();
    }

    Math::Vector3 Transform::GetRight() const
    {
        return GetRotation() * Math::Vector3::Right();
    }

    Math::Vector3 Transform::GetUp() const
    {
        return GetRotation() * Math::Vector3::Up();
    }

    void Transform::Translate(const Math::Vector3& offset, TransformSpace space)
    {
        SetPosition(GetPosition() + (space == TransformSpace::World ? offset : GetRotation() * offset));
    }

    void Transform::Rotate(const Math::Vector3& eulerDegrees, TransformSpace space)
    {
        Math::Quaternion delta = Math::Quaternion::Euler(eulerDegrees);
        if (space == TransformSpace::World)
        {
            SetRotation(delta * GetRotation());
        }
        else
        {
            SetLocalRotation(m_localRotation * delta);
        }
    }

    void Transform::RotateAround(const Math::Vector3& point, const Math::Vector3& axis, float degrees)
    {
        Math::Quaternion delta = Math::Quaternion::AngleAxis(degrees, axis);
        SetPosition(point + delta * (GetPosition() - point));
        SetRotation(delta * GetRotation());
    }

    void Transform::LookAt(const Math::Vector3& target, const Math::Vector3& up)
    {
        Math::Vector3 direction = target - GetPosition();
        if (direction.LengthSquared() > Math::Epsilon)
        {
            SetRotation(Math::Quaternion::LookRotation(direction, up));
        }
    }

    Math::Matrix4 Transform::GetLocalMatrix() const
    {
        return Math::Matrix4::TRS(m_localPosition, m_localRotation, m_localScale);
    }

    const Math::Matrix4& Transform::GetWorldMatrix() const
    {
        if (m_isWorldDirty)
        {
            const Transform* parent = GetParentTransform();
            m_worldMatrix = parent ? GetLocalMatrix() * parent->GetWorldMatrix() : GetLocalMatrix();
            m_isWorldDirty = false;
        }
        return m_worldMatrix;
    }

    Math::Matrix4 Transform::GetWorldToLocalMatrix() const
    {
        return GetWorldMatrix().Inverse();
    }

    Math::Vector3 Transform::TransformPoint(const Math::Vector3& point) const
    {
        return GetWorldMatrix().TransformPoint(point);
    }

    Math::Vector3 Transform::InverseTransformPoint(const Math::Vector3& point) const
    {
        return GetWorldToLocalMatrix().TransformPoint(point);
    }

    Math::Vector3 Transform::TransformDirection(const Math::Vector3& direction) const
    {
        return GetRotation() * direction;
    }

    Math::Vector3 Transform::InverseTransformDirection(const Math::Vector3& direction) const
    {
        return GetRotation().Inverse() * direction;
    }

    void Transform::SetWorldMatrix(const Math::Matrix4& world)
    {
        const Transform* parent = GetParentTransform();
        Math::Matrix4 local = parent ? world * parent->GetWorldMatrix().Inverse() : world;
        Math::Vector3 translation;
        Math::Quaternion rotation;
        Math::Vector3 scale;
        local.Decompose(translation, rotation, scale);
        m_localPosition = translation;
        m_localRotation = rotation;
        m_localScale = scale;
        MarkDirty();
    }
}
