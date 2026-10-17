#pragma once

#include "Core/Guid.h"
#include "Level/Level.h"
#include "Level/LevelObjectLinking.h"
#include "Level/LevelSerializationOptions.h"
#include "Serialization/Json.h"

#include <memory>

namespace BrightBox3D::Assets
{
    class Asset;
}

namespace BrightBox3D::Level
{
    class Level;
    class LevelCollection;
    class LevelObject;
}

namespace BrightBox3D::Level::LevelReader
{
    struct ReadContext
    {
        Core::Guid derivedRoot;
        Core::Guid asset;
        bool freshIds = false;
        LevelObjectLinking::SourceIndex* bySource = nullptr;
    };

    LevelObject* ReadNode(Level& level, LevelCollection* collection, LevelObject* parent, const Serialization::JsonValue& node, ReadContext& context, bool isAssetRoot);
    Core::Result<const Serialization::JsonValue*> FindAssetRoot(const Core::Guid& asset, std::shared_ptr<Assets::Asset>& holder, TemplateAccess templates);
    Core::Result<LevelObject*> InstantiateAsset(Level& level, const Core::Guid& asset, LevelCollection* collection, LevelObject* parent, const Core::Guid& instanceId, Initialization initialization, TemplateAccess templates);
    void TagForEditing(LevelObject& node);
}
