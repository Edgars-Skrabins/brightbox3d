#pragma once

#include <filesystem>
#include <string>

namespace BrightBox3D::Core
{
    class SharedLibrary
    {
    public:
        SharedLibrary() = default;
        ~SharedLibrary();

        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;

        bool Load(const std::filesystem::path& path);
        void Unload();
        bool IsLoaded() const { return m_handle != nullptr; }
        void* FindSymbol(const std::string& name) const;
        const std::string& LastError() const { return m_lastError; }

        static std::string PlatformFileName(const std::string& baseName);

    private:
        void* m_handle = nullptr;
        std::string m_lastError;
    };
}
