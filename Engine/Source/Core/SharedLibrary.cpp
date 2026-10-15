#include "Core/SharedLibrary.h"

#include "Core/EngineText.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace BrightBox3D::Core
{
    namespace
    {
#if defined(_WIN32)
        constexpr const char* LibraryPrefix = "";
        constexpr const char* LibraryExtension = ".dll";
#elif defined(__APPLE__)
        constexpr const char* LibraryPrefix = "lib";
        constexpr const char* LibraryExtension = ".dylib";
#else
        constexpr const char* LibraryPrefix = "lib";
        constexpr const char* LibraryExtension = ".so";
#endif
    }

    SharedLibrary::~SharedLibrary()
    {
        Unload();
    }

    bool SharedLibrary::Load(const std::filesystem::path& path)
    {
        Unload();
#if defined(_WIN32)
        m_handle = LoadLibraryW(path.wstring().c_str());
        if (!m_handle)
        {
            m_lastError = Text(EngineText::LoadLibraryFailed, GetLastError());
        }
#else
        m_handle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle)
        {
            const char* error = dlerror();
            m_lastError = error ? error : Text(EngineText::DlopenFailed);
        }
#endif
        return m_handle != nullptr;
    }

    void SharedLibrary::Unload()
    {
        if (!m_handle)
        {
            return;
        }
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(m_handle));
#else
        dlclose(m_handle);
#endif
        m_handle = nullptr;
    }

    void* SharedLibrary::FindSymbol(const std::string& name) const
    {
        if (!m_handle)
        {
            return nullptr;
        }
#if defined(_WIN32)
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_handle), name.c_str()));
#else
        return dlsym(m_handle, name.c_str());
#endif
    }

    std::string SharedLibrary::PlatformFileName(const std::string& baseName)
    {
        return std::string(LibraryPrefix) + baseName + LibraryExtension;
    }
}
