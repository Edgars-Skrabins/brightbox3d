#pragma once

#include "Core/SharedLibrary.h"

#include <filesystem>
#include <string>
#include <vector>

namespace BrightBox3D::Level
{
    class Level;
}

namespace BrightBox3D::Runtime
{
    class GameModule
    {
    public:
        GameModule() = default;
        ~GameModule();

        bool Load(const std::filesystem::path& library, const std::string& name, const std::filesystem::path& shadowDirectory, std::string& error);
        bool Reload(const std::filesystem::path& library, std::string& error);
        void Unload();
        bool IsLoaded() const { return m_library.IsLoaded(); }
        const std::string& GetName() const { return m_name; }
        uint32_t GetGeneration() const { return m_generation; }

    private:
        bool LoadCopy(const std::filesystem::path& library, std::string& error);

        Core::SharedLibrary m_library;
        std::string m_name;
        std::filesystem::path m_shadowDirectory;
        std::filesystem::path m_loadedCopy;
        uint32_t m_generation = 0;
    };
}
