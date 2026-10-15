#pragma once

#include "Math/Vector3.h"
#include "Physics/Rigidbody.h"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <vector>

namespace BrightBox3D::Physics
{
    class Collider;

    JPH::RefConst<JPH::Shape> CreateColliderShape(const Collider& collider, MotionType motion);
    JPH::RefConst<JPH::Shape> CombineShapes(const std::vector<JPH::RefConst<JPH::Shape>>& shapes);
    JPH::RefConst<JPH::Shape> ApplyScale(const JPH::RefConst<JPH::Shape>& shape, const Math::Vector3& scale);
}
