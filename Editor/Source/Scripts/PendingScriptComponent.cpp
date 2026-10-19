#include "Scripts/PendingScriptComponent.h"

#include "Core/Log.h"
#include "EditorContext.h"
#include "Level/LevelObjectComponent.h"
#include "Reflection/TypeRegistry.h"

namespace BrightBox3D::Editor
{
    void PendingScriptComponent::Request(const Core::Guid& object, const std::string& className)
    {
        m_request = Target{ object, className };
    }

    void PendingScriptComponent::OnReloaded(EditorContext& context)
    {
        if (!m_request)
        {
            return;
        }
        const Reflection::TypeInfo* type = Reflection::TypeRegistry::Find(m_request->className);
        if (!type || !type->IsA(Level::LevelObjectComponent::StaticType()))
        {
            return;
        }
        Level::Level* level = context.GetLevelService().GetEditLevel();
        Level::LevelObject* object = level ? level->FindObject(m_request->object) : nullptr;
        m_request.reset();
        if (!object)
        {
            return;
        }
        if (Core::Result<Level::LevelObjectComponent*> component = context.GetObjectEditService().AddComponent(*object, *type); !component)
        {
            Core::Log::Error(component.GetError().GetText());
        }
    }
}
