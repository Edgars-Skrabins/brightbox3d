#pragma once

#include "Math/Quaternion.h"
#include "Math/Vector3.h"

#include <Jolt/Jolt.h>

namespace BrightBox3D::Physics
{
    inline JPH::Vec3 ToJolt(const Math::Vector3& value)
    {
        return JPH::Vec3(value.x, value.y, value.z);
    }

    inline JPH::Quat ToJolt(const Math::Quaternion& value)
    {
        return JPH::Quat(value.x, value.y, value.z, value.w).Normalized();
    }

    inline JPH::RVec3 ToJoltPosition(const Math::Vector3& value)
    {
        return JPH::RVec3(value.x, value.y, value.z);
    }

    inline JPH::Float3 ToJoltFloat3(const Math::Vector3& value)
    {
        return JPH::Float3(value.x, value.y, value.z);
    }

    inline Math::Vector3 FromJolt(JPH::Vec3Arg value)
    {
        return { value.GetX(), value.GetY(), value.GetZ() };
    }

    inline Math::Quaternion FromJolt(JPH::QuatArg value)
    {
        return { value.GetX(), value.GetY(), value.GetZ(), value.GetW() };
    }

    inline Math::Vector3 FromJoltPosition(JPH::RVec3Arg value)
    {
        return { static_cast<float>(value.GetX()), static_cast<float>(value.GetY()), static_cast<float>(value.GetZ()) };
    }
}
