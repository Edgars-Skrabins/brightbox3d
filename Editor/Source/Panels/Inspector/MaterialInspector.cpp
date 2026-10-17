#include "Panels/Inspector/MaterialInspector.h"

#include "Assets/AssetDatabase.h"
#include "PropertyDrawer.h"
#include "Rendering/MaterialAsset.h"
#include "Serialization/ObjectSerializer.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    void MaterialInspector::Draw(const Core::Guid& guid)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        auto material = database.Load<Rendering::MaterialAsset>(guid);
        if (!material)
        {
            return;
        }
        PropertyDrawOptions options;
        options.instanceContext = false;
        options.access = database.IsBuiltin(guid) ? FieldAccess::ReadOnly : FieldAccess::Editable;
        if (PropertyDrawer::DrawObject(*material, options))
        {
            m_editing = true;
        }
        if (m_editing && !ImGui::IsAnyItemActive())
        {
            m_editing = false;
            Serialization::JsonDocument document;
            Serialization::SerializeProperties(*material, document, document.GetAllocator());
            database.SaveJsonAsset(guid, document);
        }
    }
}
