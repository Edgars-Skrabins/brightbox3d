#pragma once

#include "Core/Guid.h"
#include "Geometry/MeshData.h"

namespace BrightBox3D::Rendering
{
    class BuiltinAssets
    {
    public:
        static Core::Guid Cube() { return { 0xb0000000b0000000ull, 0x1ull }; }
        static Core::Guid Sphere() { return { 0xb0000000b0000000ull, 0x2ull }; }
        static Core::Guid Plane() { return { 0xb0000000b0000000ull, 0x3ull }; }
        static Core::Guid Quad() { return { 0xb0000000b0000000ull, 0x4ull }; }
        static Core::Guid Cylinder() { return { 0xb0000000b0000000ull, 0x5ull }; }
        static Core::Guid Capsule() { return { 0xb0000000b0000000ull, 0x6ull }; }
        static Core::Guid DefaultMaterial() { return { 0xb0000000b0000000ull, 0x10ull }; }

        static Geometry::MeshData CreateCube();
        static Geometry::MeshData CreateSphere(int segments = 32, int rings = 16);
        static Geometry::MeshData CreatePlane(int subdivisions = 10);
        static Geometry::MeshData CreateQuad();
        static Geometry::MeshData CreateCylinder(int segments = 32);
        static Geometry::MeshData CreateCapsule(int segments = 32, int rings = 8);

        static void Register();
    };
}
