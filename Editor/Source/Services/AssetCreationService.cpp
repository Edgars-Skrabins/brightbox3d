#include "Services/AssetCreationService.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Level/LevelFileSchema.h"
#include "Level/LevelSerializer.h"

#include <functional>

namespace BrightBox3D::Editor
{
    using Level::LevelObject;
    using Level::LevelSerializer;
    using Serialization::JsonDocument;
    using Serialization::JsonValue;

    Core::Guid AssetCreationService::CreateVariantAsset(LevelObject& object, const std::string& folder)
    {
        if (m_context.GetLevelService().IsPlaying())
        {
            return {};
        }
        JsonDocument before;
        ObjectEditService::Snapshot(object, before);
        JsonDocument document;
        LevelSerializer::MakeLevelObjectDocument(object, document, Assets::AssetType::LevelObjectVariant);
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string path = database.MakeUniquePath((folder.empty() ? "" : folder + "/") + object.GetName() + Assets::AssetTypeExtension(Assets::AssetType::LevelObjectVariant));
        Core::Guid asset = database.CreateJsonAsset(path, document);
        if (!asset.IsValid())
        {
            return {};
        }
        Core::Guid id = object.GetId();
        std::function<void(LevelObject&, bool)> link = [&](LevelObject& node, bool isRoot)
        {
            Level::LevelObjectLink info;
            info.asset = isRoot ? asset : Core::Guid{};
            info.sourceId = node.GetLink().sourceId.IsValid() ? node.GetLink().sourceId : node.GetId();
            info.isRoot = isRoot;
            node.SetLink(info);
            for (LevelObject* child : node.GetChildren())
            {
                link(*child, false);
            }
        };
        link(object, true);
        LevelObject* refreshed = LevelSerializer::RefreshLinkedInstance(object);
        if (refreshed)
        {
            m_context.GetUndoService().RecordObjectChange(id, before, Text(EditorText::CreateNamedType, Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant)));
            m_context.GetSelectionService().Select(refreshed->GetId());
        }
        Core::Log::Info(Text(EditorText::CreatedVariant, Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant), path));
        return asset;
    }

    Core::Guid AssetCreationService::CreateTemplateAsset(LevelObject* object, const std::string& folder)
    {
        JsonDocument document;
        Level::Level scratch;
        LevelObject* source = object;
        if (!source)
        {
            source = scratch.CreateObject(LevelObject::StaticType());
            source->SetName(Text(EditorText::NewTemplateName));
        }
        LevelSerializer::MakeLevelObjectDocument(*source, document, Assets::AssetType::LevelObjectTemplate);
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        std::string path = database.MakeUniquePath((folder.empty() ? "" : folder + "/") + source->GetName() + Assets::AssetTypeExtension(Assets::AssetType::LevelObjectTemplate));
        return database.CreateJsonAsset(path, document);
    }

    Core::Guid AssetCreationService::CreateVariantFromTemplate(const Core::Guid& templateAsset)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(templateAsset);
        if (!record || record->type != Assets::AssetType::LevelObjectTemplate)
        {
            return {};
        }
        JsonDocument linked;
        linked.SetObject();
        linked.AddMember(Level::LevelFileSchema::LevelObjectAsset, Serialization::MakeString(templateAsset.ToString(), linked.GetAllocator()), linked.GetAllocator());
        linked.AddMember(Level::LevelFileSchema::Overrides, JsonValue(rapidjson::kArrayType), linked.GetAllocator());
        Level::Level scratch;
        LevelObject* variant = LevelSerializer::ReadObject(scratch, nullptr, nullptr, linked, false);
        if (!variant)
        {
            return {};
        }
        JsonDocument document;
        LevelSerializer::MakeLevelObjectDocument(*variant, document, Assets::AssetType::LevelObjectVariant);
        std::filesystem::path templatePath(record->path);
        std::string folder = templatePath.parent_path().generic_string();
        std::string path = database.MakeUniquePath((folder.empty() ? "" : folder + "/") + record->name + " Variant" + Assets::AssetTypeExtension(Assets::AssetType::LevelObjectVariant));
        Core::Guid created = database.CreateJsonAsset(path, document);
        if (created.IsValid())
        {
            Core::Log::Info(Text(EditorText::CreatedVariantFromTemplate, Assets::AssetTypeName(Assets::AssetType::LevelObjectVariant), path, Assets::AssetTypeName(Assets::AssetType::LevelObjectTemplate)));
        }
        return created;
    }
}
