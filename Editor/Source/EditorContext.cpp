#include "EditorContext.h"

namespace BrightBox3D::Editor
{
    EditorContext::EditorContext()
        : m_project(*this), m_levels(*this), m_selection(*this), m_undo(*this), m_levelObjectEditing(*this), m_assetCreation(*this), m_objectEditing(*this)
    {
    }

    EditorContext::~EditorContext()
    {
        m_project.CloseProject();
    }
}
