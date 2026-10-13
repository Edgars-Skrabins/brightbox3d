#include "Serialization/EngineTextOverrides.h"

#include "Core/EngineText.h"
#include "Serialization/Json.h"

namespace BrightBox3D::Serialization
{
    bool LoadEngineTextOverrides(const std::filesystem::path& path, std::vector<std::string>& rejectedKeys, std::string& error)
    {
        JsonDocument document;
        if (!LoadJsonFile(path, document, &error))
        {
            return false;
        }
        if (!document.IsObject())
        {
            error = Core::Text(Core::EngineText::TextFileNotObject);
            return false;
        }
        for (auto member = document.MemberBegin(); member != document.MemberEnd(); ++member)
        {
            std::string key = member->name.GetString();
            std::optional<Core::EngineText> text = Core::FindEngineText(key);
            if (!text || !member->value.IsString() || !Core::EngineTextTable::Get().SetOverride(*text, member->value.GetString()))
            {
                rejectedKeys.push_back(key);
            }
        }
        return true;
    }
}
