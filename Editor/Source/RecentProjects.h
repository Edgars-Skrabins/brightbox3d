#pragma once

#include "EditorLayout.h"
#include "Reflection/EngineObject.h"

#include <filesystem>
#include <string>
#include <vector>

namespace BrightBox3D::Editor
{
    BrightClass()
    class RecentProjects : public Reflection::EngineObject
    {
        BrightBody()
    public:
        static constexpr size_t MaxCount = 12;
        static constexpr const char* FileName = EditorLayout::RecentProjectsFileName;

        void Load(const std::filesystem::path& path);
        void Save(const std::filesystem::path& path) const;
        void Add(const std::filesystem::path& project);
        const std::vector<std::string>& GetProjects() const { return projects; }

        BrightProperty()
        std::vector<std::string> projects;
    };
}
