#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"
#include "Runtime/Project.h"
#include "Serialization/Json.h"
#include "UndoStack.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Editor
{
    enum class SelectionFocus
    {
        None,
        Objects,
        Asset,
        Collection,
        Level
    };

    enum class GizmoOperation
    {
        Translate,
        Rotate,
        Scale
    };

    class PlaySession;

    class EditorContext
    {
    public:
        EditorContext();
        ~EditorContext();

        bool OpenProject(const std::filesystem::path& directory, std::string& error);
        void CloseProject();
        bool HasProject() const { return m_project.IsOpen(); }
        Runtime::Project& GetProject() { return m_project; }

        Level::Level* GetEditLevel() const { return m_level.get(); }
        Level::Level* GetStashedLevel() const { return m_stashedLevel.get(); }
        Level::Level* GetActiveLevel() const;
        bool OpenLevel(const Core::Guid& levelAsset);
        bool NewLevel(const std::string& relativePath);
        bool SaveLevel();
        bool IsDirty() const;
        void MarkDirty(Level::LevelCollection* collection);
        void MarkDirty(const Level::LevelObjectInstance& object);
        Level::LevelCollection* GetActiveCollection() const;
        void SetActiveCollection(Level::LevelCollection* collection);
        int GetCollectionIndex(const Level::LevelCollection* collection) const;
        Level::LevelCollection* GetCollectionByIndex(int index) const;

        bool IsEditingLevelObject() const { return m_editedLevelObject.IsValid(); }
        const Core::Guid& GetEditedLevelObject() const { return m_editedLevelObject; }
        bool OpenLevelObjectEditor(const Core::Guid& asset);
        void CloseLevelObjectEditor(bool save);
        bool SaveLevelObjectEditor();

        const std::vector<Core::Guid>& GetSelection() const { return m_selection; }
        Level::LevelObjectInstance* GetPrimarySelection() const;
        std::vector<Level::LevelObjectInstance*> GetSelectedObjects() const;
        void Select(const Core::Guid& id, bool additive = false);
        void ClearSelection();
        bool IsSelected(const Core::Guid& id) const;
        void SelectAsset(const Core::Guid& asset);
        const Core::Guid& GetSelectedAsset() const { return m_selectedAsset; }
        void SelectCollection(Level::LevelCollection* collection);
        Level::LevelCollection* GetSelectedCollection() const { return m_selectedCollection; }
        void SelectLevel();
        SelectionFocus GetFocus() const { return m_focus; }

        UndoStack& GetUndo() { return m_undo; }
        void Undo();
        void Redo();

        Level::LevelObjectInstance* CreateObject(const Reflection::TypeInfo& type, const std::string& name, Level::LevelObjectInstance* parent = nullptr);
        Level::LevelObjectInstance* CreateModelObject(const Core::Guid& model, const std::string& name, const Math::Vector3& position, Level::LevelObjectInstance* parent = nullptr);
        Level::LevelObjectInstance* InstantiateLevelObject(const Core::Guid& asset, const Math::Vector3& position, Level::LevelObjectInstance* parent = nullptr);
        Level::LevelObjectInstance* CreateLight(int lightType);
        void RecordCreation(const std::vector<Level::LevelObjectInstance*>& objects, const std::string& name);
        void DeleteSelection();
        void DuplicateSelection();
        void Reparent(Level::LevelObjectInstance& object, Level::LevelObjectInstance* parent, Level::LevelCollection* collection, int siblingIndex);
        void RecordObjectChange(const Core::Guid& id, const Serialization::JsonValue& before, const std::string& name);
        Core::Guid CreateLevelObjectAsset(Level::LevelObjectInstance& object, const std::string& folder);
        Core::Guid CreateTemplateAsset(Level::LevelObjectInstance* object, const std::string& folder);
        Core::Guid CreateLevelObjectFromTemplate(const Core::Guid& templateAsset);

        static void Snapshot(const Level::LevelObjectInstance& object, Serialization::JsonDocument& document);
        Level::LevelObjectInstance* RestoreSnapshot(const Serialization::JsonValue& snapshot, const Core::Guid& parentId, int collectionIndex, int siblingIndex);

        GizmoOperation GetGizmoOperation() const { return m_gizmoOperation; }
        void SetGizmoOperation(GizmoOperation operation) { m_gizmoOperation = operation; }
        bool GetGizmoLocal() const { return m_gizmoLocal; }
        void SetGizmoLocal(bool local) { m_gizmoLocal = local; }
        bool GetSnapping() const { return m_snapping; }
        void SetSnapping(bool snapping) { m_snapping = snapping; }

        PlaySession* GetPlaySession() const { return m_playSession; }
        void SetPlaySession(PlaySession* session) { m_playSession = session; }
        bool IsPlaying() const;

        const std::string& GetAssetsFolder() const { return m_assetsFolder; }
        void SetAssetsFolder(const std::string& folder) { m_assetsFolder = folder; }
        uint64_t GetLevelRevision() const { return m_levelRevision; }
        void BumpLevelRevision() { ++m_levelRevision; }

        std::function<void(const Core::Guid&)> OnFocusObjectRequested;

    private:
        void OnAssetChanged(const Core::Guid& guid);
        void ResetSelection();

        Runtime::Project m_project;
        std::unique_ptr<Level::Level> m_level;
        std::unique_ptr<Level::Level> m_stashedLevel;
        Core::Guid m_editedLevelObject;
        std::vector<Core::Guid> m_stashedSelection;
        std::vector<Core::Guid> m_selection;
        Core::Guid m_selectedAsset;
        Level::LevelCollection* m_selectedCollection = nullptr;
        Level::LevelCollection* m_activeCollection = nullptr;
        SelectionFocus m_focus = SelectionFocus::None;
        UndoStack m_undo;
        UndoStack m_stashedUndo;
        GizmoOperation m_gizmoOperation = GizmoOperation::Translate;
        bool m_gizmoLocal = false;
        bool m_snapping = false;
        PlaySession* m_playSession = nullptr;
        std::string m_assetsFolder;
        uint64_t m_levelRevision = 0;
        bool m_levelObjectDirty = false;
        Core::EventHandle m_assetChangedHandle = 0;
    };
}
