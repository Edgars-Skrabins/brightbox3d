#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"
#include "Math/Vector3.h"
#include "Rendering/Light.h"
#include "Serialization/Json.h"

#include <optional>
#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class ObjectEditService
    {
    public:
        explicit ObjectEditService(EditorContext& context) : m_context(context) {}

        Level::LevelObject* CreateObject(const Reflection::TypeInfo& type, const std::string& name, Level::LevelObject* parent = nullptr);
        Level::LevelObject* CreateModelObject(const Core::Guid& model, const std::string& name, const Math::Vector3& position, Level::LevelObject* parent = nullptr);
        Level::LevelObject* InstantiateLevelObject(const Core::Guid& asset, const Math::Vector3& position, Level::LevelObject* parent = nullptr);
        Level::LevelObject* CreateLight(Rendering::LightType type);
        void DeleteSelection();
        void DuplicateSelection();
        void Reparent(Level::LevelObject& object, Level::LevelObject* parent, Level::LevelCollection* collection, std::optional<size_t> siblingPosition);

        static void Snapshot(const Level::LevelObject& object, Serialization::JsonDocument& document);
        Level::LevelObject* RestoreSnapshot(const Serialization::JsonValue& snapshot, const Core::Guid& parentId, std::optional<size_t> collectionIndex, size_t siblingIndex);

    private:
        EditorContext& m_context;
    };
}
