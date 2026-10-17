#pragma once

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Reflection
{
    struct ValueDescriptor;
}

namespace BrightBox3D::Editor
{
    enum class FieldAccess
    {
        Editable,
        ReadOnly
    };
}

namespace BrightBox3D::Editor::ValueDrawers
{
    bool Draw(const char* label, const Reflection::ValueDescriptor& descriptor, void* value, FieldAccess access, const Level::Level* level);
}
