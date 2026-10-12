#include "Core/SharedLibrary.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace BrightBox3D::Core
{
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
            m_lastError = "LoadLibrary failed with error " + std::to_string(GetLastError());
        }
#else
        m_handle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle)
        {
            const char* error = dlerror();
            m_lastError = error ? error : "dlopen failed";
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
#if defined(_WIN32)
        return baseName + ".dll";
#elif defined(__APPLE__)
        return "lib" + baseName + ".dylib";
#else
        return "lib" + baseName + ".so";
#endif
    }
}
