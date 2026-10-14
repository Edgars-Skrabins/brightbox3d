#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace BrightBox3D::Editor
{
    class RecentProjects;

    class ProjectLauncher
    {
    public:
        explicit ProjectLauncher(const RecentProjects& recentProjects);

        std::optional<std::filesystem::path> Draw();
        void SetError(const std::string& error) { m_error = error; }
        void ClearError() { m_error.clear(); }

    private:
        void DrawRecentProjects(std::optional<std::filesystem::path>& toOpen);
        void DrawNewProject(std::optional<std::filesystem::path>& toOpen);
        void DrawOpenProject(std::optional<std::filesystem::path>& toOpen);

        const RecentProjects& m_recentProjects;
        std::string m_openPath;
        std::string m_name = "MyGame";
        std::string m_location;
        std::string m_error;
    };
}
