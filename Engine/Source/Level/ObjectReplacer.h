#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"
#include "Level/LevelObject.h"
#include "Level/Transform.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class LevelCollection;

    struct ReplacedComponent
    {
        std::unique_ptr<LevelObjectComponent> kept;
        std::unique_ptr<LevelObjectComponent> doomed;
        std::string typeName;
        std::string data;
        ComponentRuntimeState state;
        size_t index = 0;
        const Reflection::EngineObject* oldAddress = nullptr;
        Reflection::ReferenceList references;
    };

    struct ReplacedObject
    {
        Level* level = nullptr;
        Core::Guid id;
        Core::Guid parentId;
        LevelCollection* collection = nullptr;
        size_t siblingIndex = 0;
        int depth = 0;
        bool replaceBrain = false;
        std::string typeName;
        std::string brainData;
        std::unique_ptr<LevelObject> oldBrain;
        const Reflection::EngineObject* oldAddress = nullptr;
        Reflection::ReferenceList references;
        LevelObjectRuntimeState state;
        Transform transform;
        ComponentRuntimeState transformState;
        std::vector<LevelObject*> children;
        std::vector<ReplacedComponent> components;
    };

    struct ReplacedEngineObject
    {
        Level* level = nullptr;
        size_t index = 0;
        std::vector<std::string> typeNames;
        std::string data;
        Reflection::EngineObjectState state;
        std::unique_ptr<Reflection::EngineObject> old;
        const Reflection::EngineObject* oldAddress = nullptr;
        Reflection::ReferenceList references;
    };

    struct ReplacementBatch
    {
        std::vector<ReplacedObject> objects;
        std::vector<ReplacedEngineObject> engineObjects;

        size_t Count() const { return objects.size() + engineObjects.size(); }
    };

    class ObjectReplacer
    {
    public:
        using Predicate = std::function<bool(const Reflection::TypeInfo&)>;

        static void Extract(Level& level, const Predicate& isReplaced, ReplacementBatch& batch);
        static void DestroyExtracted(ReplacementBatch& batch);
        static void Restore(ReplacementBatch& batch);
    };
}
