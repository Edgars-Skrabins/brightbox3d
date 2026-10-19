#include "Services/ScriptLibrary.h"

#include "Core/Hash.h"

#include <algorithm>
#include <map>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr std::string_view ScriptIdSalt = "Script";
        constexpr std::string_view FolderIdSalt = "ScriptFolder";

        Core::Guid MakeId(std::string_view salt, const std::string& path)
        {
            return { Core::HashString(path), Core::HashCombine(Core::HashString(salt), Core::HashString(path)) };
        }

        std::string ParentOf(const std::string& folder)
        {
            return std::filesystem::path(folder).parent_path().generic_string();
        }

        bool IsHidden(const std::filesystem::path& path)
        {
            std::string name = path.filename().string();
            return !name.empty() && name.front() == '.';
        }
    }

    void ScriptLibrary::SetRoot(const std::filesystem::path& root)
    {
        m_root = root;
        m_scripts.clear();
        m_folders.clear();
        m_dirty = true;
    }

    void ScriptLibrary::Refresh()
    {
        m_scripts.clear();
        m_folders.clear();
        m_dirty = false;
        std::error_code error;
        if (m_root.empty() || !std::filesystem::is_directory(m_root, error))
        {
            return;
        }
        std::map<std::string, ScriptEntry> byPath;
        for (std::filesystem::recursive_directory_iterator it(m_root, error), end; !error && it != end; it.increment(error))
        {
            if (IsHidden(it->path()))
            {
                if (it->is_directory())
                {
                    it.disable_recursion_pending();
                }
                continue;
            }
            std::string relative = std::filesystem::relative(it->path(), m_root, error).generic_string();
            if (it->is_directory())
            {
                m_folders.push_back(relative);
                continue;
            }
            std::string extension = it->path().extension().string();
            if (extension != HeaderExtension && extension != SourceExtension)
            {
                continue;
            }
            std::string folder = ParentOf(relative);
            std::string className = it->path().stem().string();
            std::string key = folder.empty() ? className : folder + "/" + className;
            ScriptEntry& entry = byPath[key];
            entry.id = MakeId(ScriptIdSalt, key);
            entry.folder = folder;
            entry.className = className;
            (extension == HeaderExtension ? entry.header : entry.source) = it->path();
        }
        for (auto& [key, entry] : byPath)
        {
            m_scripts.push_back(std::move(entry));
        }
        std::sort(m_folders.begin(), m_folders.end());
    }

    void ScriptLibrary::RefreshIfDirty()
    {
        if (m_dirty)
        {
            Refresh();
        }
    }

    const std::vector<ScriptEntry>& ScriptLibrary::GetScripts()
    {
        RefreshIfDirty();
        return m_scripts;
    }

    const std::vector<std::string>& ScriptLibrary::GetFolders()
    {
        RefreshIfDirty();
        return m_folders;
    }

    std::vector<std::string> ScriptLibrary::ChildFolders(const std::string& folder)
    {
        RefreshIfDirty();
        std::vector<std::string> children;
        for (const std::string& candidate : m_folders)
        {
            if (ParentOf(candidate) == folder)
            {
                children.push_back(candidate);
            }
        }
        return children;
    }

    std::vector<ScriptEntry> ScriptLibrary::ScriptsIn(const std::string& folder)
    {
        RefreshIfDirty();
        std::vector<ScriptEntry> scripts;
        for (const ScriptEntry& entry : m_scripts)
        {
            if (entry.folder == folder)
            {
                scripts.push_back(entry);
            }
        }
        return scripts;
    }

    const ScriptEntry* ScriptLibrary::Find(const Core::Guid& id)
    {
        RefreshIfDirty();
        auto it = std::find_if(m_scripts.begin(), m_scripts.end(), [&id](const ScriptEntry& entry) { return entry.id == id; });
        return it == m_scripts.end() ? nullptr : &*it;
    }

    const ScriptEntry* ScriptLibrary::FindByClass(const std::string& className)
    {
        RefreshIfDirty();
        auto it = std::find_if(m_scripts.begin(), m_scripts.end(), [&className](const ScriptEntry& entry) { return entry.className == className; });
        return it == m_scripts.end() ? nullptr : &*it;
    }

    bool ScriptLibrary::HasFolder(const std::string& folder)
    {
        RefreshIfDirty();
        return folder.empty() || std::find(m_folders.begin(), m_folders.end(), folder) != m_folders.end();
    }

    std::optional<std::string> ScriptLibrary::FindFolder(const Core::Guid& id)
    {
        RefreshIfDirty();
        auto it = std::find_if(m_folders.begin(), m_folders.end(), [&id](const std::string& folder) { return FolderId(folder) == id; });
        if (it == m_folders.end())
        {
            return std::nullopt;
        }
        return *it;
    }

    std::filesystem::path ScriptLibrary::FolderPath(const std::string& folder) const
    {
        return folder.empty() ? m_root : m_root / folder;
    }

    Core::Guid ScriptLibrary::FolderId(const std::string& folder)
    {
        return MakeId(FolderIdSalt, folder);
    }
}
