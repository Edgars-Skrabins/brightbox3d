#pragma once

#include "Math/Geometry.h"

namespace BrightBox3D::Editor
{
    class EditorContext;
}

namespace BrightBox3D::Editor::LevelViewDropTarget
{
    void Handle(EditorContext& context, const Math::Ray& mouseRay);
}
