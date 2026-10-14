#pragma once

#include "Core/Guid.h"

namespace BrightBox3D::Editor
{
    class MaterialInspector
    {
    public:
        void Draw(const Core::Guid& guid);

    private:
        bool m_editing = false;
    };
}
