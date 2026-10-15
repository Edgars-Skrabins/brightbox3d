#include "Panels/Inspector/AssetInspector.h"

#include "Assets/AssetDatabase.h"
#include "Assets/TextAsset.h"
#include "EditorApplication.h"
#include "EditorText.h"
#include "Geometry/ModelAsset.h"
#include "ImGui/ImGuiRenderer.h"
#include "Labels.h"
#include "Rendering/TextureAsset.h"
#include "Thumbnails.h"

#include <imgui.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr float ThumbnailSize = 64.0f;
        constexpr size_t TriangleIndexCount = 3;

        std::string StemOf(const std::string& name)
        {
            return name.substr(0, name.find('.'));
        }
    }

    AssetInspector::AssetInspector(EditorApplication& application) : m_application(application)
    {
    }

    void AssetInspector::DrawHeader(const Assets::AssetRecord& record)
    {
        if (RHI::Texture* thumbnail = m_application.GetThumbnails().Get(record.guid))
        {
            ImGui::Image(ImGuiRenderer::ToTextureId(thumbnail), ImVec2(ThumbnailSize, ThumbnailSize));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::TextUnformatted(record.name.c_str());
        ImGui::TextDisabled("%s", Assets::AssetTypeName(record.type));
        ImGui::TextDisabled("%s", record.path.c_str());
        ImGui::EndGroup();
        ImGui::Separator();
    }

    void AssetInspector::DrawLevel(const Core::Guid& guid)
    {
        EditorContext& context = m_application.GetContext();
        if (ImGui::Button(Text(EditorText::OpenLevel)))
        {
            context.GetLevelService().OpenLevel(guid);
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::SetStartupLevel)))
        {
            context.GetProjectService().GetProject().SetStartupLevel(guid);
            context.GetProjectService().GetProject().Save();
        }
    }

    void AssetInspector::DrawVariant(const Core::Guid& guid)
    {
        EditorContext& context = m_application.GetContext();
        if (ImGui::Button(Text(EditorText::OpenLevelObject, TypeName<Level::LevelObject>()).c_str()))
        {
            context.GetLevelObjectEditService().OpenLevelObjectEditor(guid);
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::PlaceInLevel)))
        {
            context.GetObjectEditService().InstantiateLevelObject(guid, m_application.GetSpawnPoint());
        }
    }

    void AssetInspector::DrawTemplate(const Core::Guid& guid)
    {
        EditorContext& context = m_application.GetContext();
        ImGui::TextWrapped("%s", Text(EditorText::TemplateExplanation, Assets::AssetTypeName(Assets::AssetType::LevelObjectTemplate), TypeName<Level::LevelObject>()).c_str());
        if (ImGui::Button(Text(EditorText::EditTemplate)))
        {
            context.GetLevelObjectEditService().OpenLevelObjectEditor(guid);
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::CreateVariant)))
        {
            Core::Guid created = context.GetAssetCreationService().CreateVariantFromTemplate(guid);
            if (created.IsValid())
            {
                context.GetSelectionService().SelectAsset(created);
            }
        }
    }

    void AssetInspector::DrawTexture(const Core::Guid& guid)
    {
        if (auto texture = Assets::AssetDatabase::Get().Load<Rendering::TextureAsset>(guid))
        {
            ImGui::TextUnformatted(Text(EditorText::TextureSummary, texture->GetWidth(), texture->GetHeight(), texture->GetMips().size()).c_str());
        }
        m_importSettings.Draw(guid);
    }

    void AssetInspector::DrawModel(const Assets::AssetRecord& record)
    {
        if (auto model = Assets::AssetDatabase::Get().Load<Geometry::ModelAsset>(record.guid))
        {
            size_t triangles = 0;
            for (const Geometry::MeshData& mesh : model->GetMeshes())
            {
                triangles += mesh.indices.size() / TriangleIndexCount;
            }
            ImGui::TextUnformatted(Text(EditorText::ModelSummary, model->GetMeshes().size(), model->GetNodes().size(), triangles).c_str());
            if (ImGui::Button(Text(EditorText::PlaceInLevel)))
            {
                m_application.GetContext().GetObjectEditService().CreateModelObject(record.guid, StemOf(record.name), m_application.GetSpawnPoint());
            }
        }
        m_importSettings.Draw(record.guid);
    }

    void AssetInspector::DrawTextAsset(const Assets::AssetRecord& record)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (ImGui::Button(Text(EditorText::OpenInCodeEditor)))
        {
            m_application.OpenFileInEditor(database.GetAbsolutePath(record).string(), 0);
        }
        if (auto text = database.Load<Assets::TextAsset>(record.guid))
        {
            ImGui::BeginChild("Preview", ImVec2(0, 0), ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(text->GetText().c_str());
            ImGui::EndChild();
        }
    }

    void AssetInspector::Draw(const Core::Guid& guid)
    {
        const Assets::AssetRecord* found = Assets::AssetDatabase::Get().FindRecord(guid);
        if (!found)
        {
            ImGui::TextDisabled("%s", Text(EditorText::AssetNotFound));
            return;
        }
        Assets::AssetRecord record = *found;
        DrawHeader(record);
        switch (record.type)
        {
        case Assets::AssetType::Level:
            DrawLevel(guid);
            break;
        case Assets::AssetType::LevelObjectVariant:
            DrawVariant(guid);
            break;
        case Assets::AssetType::LevelObjectTemplate:
            DrawTemplate(guid);
            break;
        case Assets::AssetType::Material:
            m_material.Draw(guid);
            break;
        case Assets::AssetType::Texture:
            DrawTexture(guid);
            break;
        case Assets::AssetType::Model:
            DrawModel(record);
            break;
        case Assets::AssetType::Screen:
        case Assets::AssetType::Widget:
        case Assets::AssetType::StyleSheet:
            DrawTextAsset(record);
            break;
        default:
            break;
        }
    }
}
