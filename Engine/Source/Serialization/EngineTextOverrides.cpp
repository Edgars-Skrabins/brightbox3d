#include "Serialization/EngineTextOverrides.h"

#include "Core/EngineText.h"
#include "Serialization/Json.h"

namespace BrightBox3D::Serialization
{
    Core::Result<std::vector<std::string>> LoadEngineTextOverrides(const std::filesystem::path& path)
    {
        JsonDocument document;
        if (Core::Result<void> loaded = LoadJsonFile(path, document); !loaded)
        {
            return loaded.GetError();
        }
        if (!document.IsObject())
        {
            return Core::Error(Core::EngineText::TextFileNotObject);
        }
        std::vector<std::string> rejectedKeys;
        for (auto member = document.MemberBegin(); member != document.MemberEnd(); ++member)
        {
            std::string key = member->name.GetString();
            std::optional<Core::EngineText> text = Core::FindEngineText(key);
            if (!text || !member->value.IsString() || !Core::EngineTextTable::Get().SetOverride(*text, member->value.GetString()))
            {
                rejectedKeys.push_back(key);
            }
        }
        return rejectedKeys;
    }
}
