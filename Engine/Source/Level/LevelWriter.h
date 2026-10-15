#pragma once

#include "Level/LevelSerializationOptions.h"
#include "Serialization/Json.h"

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Level::LevelWriter
{
    void WriteNode(const LevelObject& object, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator, LinkWriting links, NodeWriting nodes);
    void WriteLinkedNode(const LevelObject& root, Serialization::JsonValue& out, Serialization::JsonAllocator& allocator);
}
