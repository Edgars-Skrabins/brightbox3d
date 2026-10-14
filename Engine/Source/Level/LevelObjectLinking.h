#pragma once

#include "Core/Guid.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace BrightBox3D::Level
{
    class LevelObject;
}

namespace BrightBox3D::Level::LevelObjectLinking
{
    using SourceIndex = std::unordered_map<Core::Guid, LevelObject*>;

    bool IsInSameLink(const LevelObject& object, const LevelObject& linkRoot);
    void CollectLinkedSubtree(LevelObject& linkRoot, LevelObject& object, std::vector<LevelObject*>& linked, std::vector<std::pair<LevelObject*, LevelObject*>>& foreign);
    std::vector<LevelObject*> CollectLinked(LevelObject& linkRoot);
    SourceIndex IndexBySource(LevelObject& root);
    SourceIndex IndexLinkedBySource(LevelObject& linkRoot);
}
