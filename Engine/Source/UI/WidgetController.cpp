#include "UI/WidgetController.h"

#include "Level/Level.h"
#include "UI/Document.h"

namespace BrightBox3D::UI
{
    Element* WidgetController::Find(const std::string& name) const
    {
        return m_root ? m_root->FindByName(name) : nullptr;
    }

    Level::LevelObject* WidgetController::GetPlayer() const
    {
        return Reflection::Cast<Level::LevelObject>(GetBindingRoot(BindingRoot::Player));
    }

    Level::Level* WidgetController::GetLevel() const
    {
        return m_document ? m_document->GetRoots().level : nullptr;
    }

    Reflection::EngineObject* WidgetController::GetBindingRoot(BindingRoot root) const
    {
        return m_document ? m_document->GetRoots().Get(root) : nullptr;
    }
}
