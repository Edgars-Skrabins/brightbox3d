#pragma once

#include "Serialization/Json.h"

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Level::LevelWriter
{
    void WriteNode(const LevelObject& object, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator, bool preserveLinks, bool assetMode);
    void WriteLinkedNode(const LevelObject& root, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
}
