#include "Panels/Inspector/ImportSettingsInspector.h"

#include "Assets/AssetDatabase.h"
#include "EditorText.h"
#include "PropertyDrawer.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    void ImportSettingsInspector::Draw(const Core::Guid& guid)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record || database.IsBuiltin(guid))
        {
            return;
        }
        if (m_asset != guid)
        {
            m_asset = guid;
            m_settings = database.LoadImportSettings(*record);
            m_dirty = false;
        }
        if (!m_settings)
        {
            return;
        }
        ImGui::SeparatorText(Text(EditorText::ImportSettings));
        PropertyDrawOptions options;
        options.instanceContext = false;
        if (PropertyDrawer::DrawObject(*m_settings, options))
        {
            m_dirty = true;
        }
        ImGui::BeginDisabled(!m_dirty);
        if (ImGui::Button(Text(EditorText::Apply)))
        {
            database.SaveImportSettings(guid, *m_settings);
            m_dirty = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Revert)))
        {
            m_settings = database.LoadImportSettings(*record);
            m_dirty = false;
        }
        ImGui::EndDisabled();
    }
}
