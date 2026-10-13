#include "Runtime/GameManifest.h"

namespace BrightBox3D::Runtime
{
    bool GameManifest::Load(const std::filesystem::path& path, std::string& error)
    {
        Serialization::JsonDocument document;
        if (!Serialization::LoadJsonFile(path, document, &error) || !document.IsObject())
        {
            if (error.empty())
            {
                error = "invalid game manifest";
            }
            return false;
        }
        name = Serialization::GetString(document, "name", "Game");
        gameModule = Serialization::GetString(document, "gameModule");
        startupLevel = Core::Guid::Parse(Serialization::GetString(document, "startupLevel"));
        fixedTimeStep = static_cast<float>(Serialization::GetNumber(document, "fixedTimeStep", 1.0 / 60.0));
        gameMode.SetObject();
        if (const Serialization::JsonValue* mode = Serialization::FindMember(document, "gameMode"))
        {
            gameMode.CopyFrom(*mode, gameMode.GetAllocator());
        }
        return true;
    }

    bool GameManifest::Save(const std::filesystem::path& path) const
    {
        Serialization::JsonDocument document;
        document.SetObject();
        auto& allocator = document.GetAllocator();
        document.AddMember("name", Serialization::MakeString(name, allocator), allocator);
        document.AddMember("gameModule", Serialization::MakeString(gameModule, allocator), allocator);
        document.AddMember("startupLevel", Serialization::MakeString(startupLevel.ToString(), allocator), allocator);
        document.AddMember("fixedTimeStep", static_cast<double>(fixedTimeStep), allocator);
        Serialization::JsonValue mode;
        mode.CopyFrom(gameMode, allocator);
        document.AddMember("gameMode", mode, allocator);
        return Serialization::SaveJsonFile(path, document);
    }
}
