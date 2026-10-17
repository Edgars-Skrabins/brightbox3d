#pragma once

#include "Runtime/Project.h"

#include <filesystem>
#include <string>

namespace BrightBox3D::Editor
{
    class EditorContext;

    class ProjectService
    {
    public:
        explicit ProjectService(EditorContext& context) : m_context(context) {}

        Core::Result<void> OpenProject(const std::filesystem::path& directory);
        void CloseProject();
        bool HasProject() const { return m_project.IsOpen(); }
        Runtime::Project& GetProject() { return m_project; }
        const Runtime::Project& GetProject() const { return m_project; }
        bool SaveProjectSettings() const { return m_project.Save(); }

    private:
        EditorContext& m_context;
        Runtime::Project m_project;
    };
}
