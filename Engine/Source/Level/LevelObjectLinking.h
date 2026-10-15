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
    template <typename Object>
    void CollectLinkedSubtree(Object& linkRoot, Object& object, std::vector<Object*>& linked, std::vector<std::pair<Object*, Object*>>& foreign);

    template <typename Object>
    std::vector<Object*> CollectLinked(Object& linkRoot);
    SourceIndex IndexBySource(LevelObject& root);
    SourceIndex IndexLinkedBySource(LevelObject& linkRoot);
}
