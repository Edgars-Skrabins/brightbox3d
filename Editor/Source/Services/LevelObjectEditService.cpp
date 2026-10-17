#include "Services/LevelObjectEditService.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "EditorContext.h"
#include "EditorText.h"
#include "Level/LevelAssets.h"
#include "Level/LevelSerializer.h"

namespace BrightBox3D::Editor
{
    LevelObjectEditService::LevelObjectEditService(EditorContext& context) : m_context(context)
    {
        m_assetChangedHandle = Assets::AssetDatabase::Get().OnAssetChanged.Subscribe([this](const Core::Guid& guid) { OnAssetChanged(guid); });
    }

    LevelObjectEditService::~LevelObjectEditService()
    {
        Assets::AssetDatabase::Get().OnAssetChanged.Unsubscribe(m_assetChangedHandle);
    }

    void LevelObjectEditService::Reset()
    {
        m_stashedLevel.reset();
        m_stashedSelection.clear();
        m_stashedUndo.Clear();
        m_editedLevelObject = {};
        m_dirty = false;
    }

    bool LevelObjectEditService::OpenLevelObjectEditor(const Core::Guid& asset)
    {
        if (m_context.GetLevelService().IsPlaying())
        {
            return false;
        }
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(asset);
        if (!record)
        {
            Core::Log::Error(Text(EditorText::CannotOpenAssetForEditing));
            return false;
        }
        if (IsEditingLevelObject())
        {
            CloseLevelObjectEditor(EditorChanges::Save);
        }
        auto level = std::make_unique<Level::Level>(record->name);
        Level::LevelCollection* collection = level->AddCollection(record->name);
        Level::LevelObject* object = Level::LevelSerializer::OpenAsset(*level, collection, asset);
        if (!object)
        {
            Core::Log::Error(Text(EditorText::CannotOpenAssetForEditing));
            return false;
        }
        level->InitializeHierarchy(*object);

        SelectionService& selection = m_context.GetSelectionService();
        m_stashedSelection = selection.GetSelection();
        m_stashedUndo = m_context.GetUndoService().ExchangeStack(UndoStack());
        m_stashedLevel = m_context.GetLevelService().ExchangeEditLevel(std::move(level));
        m_editedLevelObject = asset;
        m_dirty = false;
        selection.Reset();
        selection.Select(object->GetId());
        return true;
    }

    bool LevelObjectEditService::SaveLevelObjectEditor()
    {
        Level::Level* level = m_context.GetLevelService().GetEditLevel();
        if (!IsEditingLevelObject() || !level || level->GetCollections().empty() || level->GetCollections().front()->GetRoots().empty())
        {
            return false;
        }
        Level::LevelObject* root = level->GetCollections().front()->GetRoots().front();
        bool saved = Level::LevelSerializer::SaveAsset(*root, m_editedLevelObject);
        if (saved)
        {
            m_dirty = false;
            Core::Log::Info(Text(EditorText::Saved, level->GetName()));
        }
        return saved;
    }

    void LevelObjectEditService::CloseLevelObjectEditor(EditorChanges changes)
    {
        if (!IsEditingLevelObject())
        {
            return;
        }
        if (changes == EditorChanges::Save && m_dirty)
        {
            SaveLevelObjectEditor();
        }
        m_context.GetLevelService().ExchangeEditLevel(std::move(m_stashedLevel));
        m_context.GetUndoService().ExchangeStack(std::move(m_stashedUndo));
        m_stashedUndo = UndoStack();
        m_editedLevelObject = {};
        SelectionService& selection = m_context.GetSelectionService();
        selection.Reset();
        selection.SetSelection(std::move(m_stashedSelection));
        m_stashedSelection.clear();
    }

    void LevelObjectEditService::OnAssetChanged(const Core::Guid& guid)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(guid);
        if (!record)
        {
            return;
        }
        std::vector<Core::Guid> variants;
        if (record->type == Assets::AssetType::LevelObjectVariant)
        {
            variants.push_back(guid);
        }
        else if (record->type == Assets::AssetType::LevelObjectTemplate)
        {
            Level::LevelObjectLibrary::Get().Invalidate(guid);
            for (const Assets::AssetRecord& candidate : database.GetRecords(Assets::AssetType::LevelObjectVariant))
            {
                auto variant = database.Load<Level::LevelObjectVariantAsset>(candidate.guid);
                if (!variant)
                {
                    Assets::ReportLoadFailure(variant.GetError());
                }
                else if (variant->GetParentTemplate() == guid)
                {
                    variants.push_back(candidate.guid);
                }
            }
        }
        LevelService& levels = m_context.GetLevelService();
        Level::Level* target = IsEditingLevelObject() ? m_stashedLevel.get() : levels.GetEditLevel();
        for (const Core::Guid& variant : variants)
        {
            if (target)
            {
                Level::LevelSerializer::RefreshLinkedInstances(*target, variant);
            }
            else
            {
                Level::LevelObjectLibrary::Get().Invalidate(variant);
            }
        }
        if (IsEditingLevelObject() && levels.GetEditLevel() && record->type == Assets::AssetType::LevelObjectTemplate)
        {
            Level::LevelSerializer::RefreshLinkedInstances(*levels.GetEditLevel(), guid);
        }
        if (target && !variants.empty())
        {
            levels.BumpLevelRevision();
        }
    }
}
