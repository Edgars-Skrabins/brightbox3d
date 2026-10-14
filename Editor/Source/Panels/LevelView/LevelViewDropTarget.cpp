#include "Panels/LevelView/LevelViewDropTarget.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "DragDrop.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Picking.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"

#include <imgui.h>

namespace BrightBox3D::Editor::LevelViewDropTarget
{
    namespace
    {
        constexpr float MaxGroundDistance = 500.0f;
        constexpr float FallbackPlacementDistance = 8.0f;

        Math::Vector3 PlacementPoint(EditorContext& context, const Math::Ray& ray)
        {
            if (Level::Level* level = context.GetLevelService().GetActiveLevel())
            {
                PickResult hit = PickObject(*level, ray);
                if (hit.object)
                {
                    return hit.point;
                }
            }
            Math::Plane ground{ { 0.0f, 1.0f, 0.0f }, 0.0f };
            float distance = 0.0f;
            if (ground.Raycast(ray, distance) && distance < MaxGroundDistance)
            {
                return ray.GetPoint(distance);
            }
            return ray.GetPoint(FallbackPlacementDistance);
        }

        void AssignMaterial(EditorContext& context, const Core::Guid& material, const Math::Ray& ray)
        {
            Level::Level* level = context.GetLevelService().GetActiveLevel();
            PickResult hit = level ? PickObject(*level, ray) : PickResult{};
            if (!hit.object)
            {
                return;
            }
            if (auto* renderer = hit.object->GetComponent<Rendering::MeshRenderer>())
            {
                Serialization::JsonDocument before;
                ObjectEditService::Snapshot(*hit.object, before);
                renderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(material));
                context.GetUndoService().RecordObjectChange(hit.object->GetId(), before, Text(EditorText::UndoAssignMaterial));
            }
        }

        void DropAsset(EditorContext& context, const Assets::AssetRecord& record, const Math::Ray& ray)
        {
            Math::Vector3 point = PlacementPoint(context, ray);
            switch (record.type)
            {
            case Assets::AssetType::Model:
                context.GetObjectEditService().CreateModelObject(record.guid, record.name.substr(0, record.name.find('.')), point);
                break;
            case Assets::AssetType::LevelObjectVariant:
                context.GetObjectEditService().InstantiateLevelObject(record.guid, point);
                break;
            case Assets::AssetType::LevelObjectTemplate:
                Core::Log::Warning(Text(EditorText::CreateVariantFirst));
                break;
            case Assets::AssetType::Material:
                AssignMaterial(context, record.guid, ray);
                break;
            case Assets::AssetType::Level:
                context.GetLevelService().OpenLevel(record.guid);
                break;
            default:
                break;
            }
        }
    }

    void Handle(EditorContext& context, const Math::Ray& mouseRay)
    {
        if (!ImGui::BeginDragDropTarget())
        {
            return;
        }
        if (std::optional<Core::Guid> dropped = AcceptPayload(DragDropPayload::Asset))
        {
            if (const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(*dropped))
            {
                Assets::AssetRecord copy = *record;
                DropAsset(context, copy, mouseRay);
            }
        }
        ImGui::EndDragDropTarget();
    }
}
