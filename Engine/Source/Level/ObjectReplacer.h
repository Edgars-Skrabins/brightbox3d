#pragma once

#include "Core/Guid.h"
#include "Level/LevelObjectInstance.h"
#include "Math/Quaternion.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
    class LevelCollection;

    struct ReplacedComponent
    {
        std::unique_ptr<Component> kept;
        std::unique_ptr<Component> doomed;
        std::string typeName;
        std::string data;
        Core::Guid localId;
        bool enabled = true;
        bool hasLoaded = false;
        bool hasStarted = false;
        bool enabledState = false;
        size_t index = 0;
    };

    struct ReplacedObject
    {
        Level* level = nullptr;
        Core::Guid id;
        Core::Guid parentId;
        LevelCollection* collection = nullptr;
        int siblingIndex = 0;
        int depth = 0;
        bool replaceBrain = false;
        std::string typeName;
        std::string brainData;
        std::unique_ptr<LevelObjectInstance> oldBrain;
        std::string name;
        bool active = true;
        LevelObjectLink link;
        Core::Guid ownerId;
        bool hideInLevelContents = false;
        bool hasLoaded = false;
        bool hasStarted = false;
        bool enabledState = false;
        bool pendingDestroy = false;
        Math::Vector3 localPosition;
        Math::Quaternion localRotation;
        Math::Vector3 localScale{ 1.0f, 1.0f, 1.0f };
        Core::Guid transformLocalId;
        std::vector<LevelObjectInstance*> children;
        std::vector<ReplacedComponent> components;
        size_t componentCount = 0;
    };

    class ObjectReplacer
    {
    public:
        using Predicate = std::function<bool(const Reflection::TypeInfo&)>;

        static void Extract(Level& level, const Predicate& isReplaced, std::vector<ReplacedObject>& records);
        static void DestroyExtracted(std::vector<ReplacedObject>& records);
        static void Restore(std::vector<ReplacedObject>& records);
    };
}
