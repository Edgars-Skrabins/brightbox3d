#pragma once

#include "Core/Guid.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace BrightBox3D::Editor
{
    struct ScriptEntry
    {
        Core::Guid id;
        std::string folder;
        std::string className;
        std::filesystem::path header;
        std::filesystem::path source;

        const std::filesystem::path& PrimaryFile() const { return source.empty() ? header : source; }
    };

    class ScriptLibrary
    {
    public:
        static constexpr const char* HeaderExtension = ".h";
        static constexpr const char* SourceExtension = ".cpp";

        void SetRoot(const std::filesystem::path& root);
        const std::filesystem::path& GetRoot() const { return m_root; }
        void MarkDirty() { m_dirty = true; }
        void Refresh();

        const std::vector<ScriptEntry>& GetScripts();
        const std::vector<std::string>& GetFolders();
        std::vector<std::string> ChildFolders(const std::string& folder);
        std::vector<ScriptEntry> ScriptsIn(const std::string& folder);
        const ScriptEntry* Find(const Core::Guid& id);
        const ScriptEntry* FindByClass(const std::string& className);
        bool HasFolder(const std::string& folder);
        std::optional<std::string> FindFolder(const Core::Guid& id);
        std::filesystem::path FolderPath(const std::string& folder) const;

        static Core::Guid FolderId(const std::string& folder);

    private:
        void RefreshIfDirty();

        std::filesystem::path m_root;
        std::vector<ScriptEntry> m_scripts;
        std::vector<std::string> m_folders;
        bool m_dirty = true;
    };
}
