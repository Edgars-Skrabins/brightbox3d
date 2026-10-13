#include "RecentProjects.h"

#include "Runtime/Project.h"
#include "Serialization/Json.h"
#include "Serialization/ObjectSerializer.h"

#include <algorithm>

namespace BrightBox3D::Editor
{
    void RecentProjects::Load(const std::filesystem::path& path)
    {
        projects.clear();
        Serialization::JsonDocument document;
        if (!Serialization::LoadJsonFile(path, document))
        {
            return;
        }
        if (document.IsArray())
        {
            for (const Serialization::JsonValue& entry : document.GetArray())
            {
                if (entry.IsString())
                {
                    projects.push_back(entry.GetString());
                }
            }
        }
        else
        {
            Serialization::DeserializeProperties(*this, document);
        }
        std::erase_if(projects, [](const std::string& project) { return !Runtime::Project::IsProjectDirectory(project); });
    }

    void RecentProjects::Save(const std::filesystem::path& path) const
    {
        Serialization::JsonDocument document;
        document.SetObject();
        Serialization::SerializeProperties(*this, document, document.GetAllocator());
        Serialization::SaveJsonFile(path, document);
    }

    void RecentProjects::Add(const std::filesystem::path& project)
    {
        std::string text = project.generic_string();
        std::erase(projects, text);
        projects.insert(projects.begin(), text);
        if (projects.size() > MaxCount)
        {
            projects.resize(MaxCount);
        }
    }
}
