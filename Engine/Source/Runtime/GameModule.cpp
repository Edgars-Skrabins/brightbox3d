#include "Runtime/GameModule.h"

#include "Core/Log.h"
#include "Level/Level.h"
#include "Level/LevelSerializer.h"
#include "Level/ObjectReplacer.h"
#include "Reflection/TypeRegistry.h"

namespace BrightBox3D::Runtime
{
    GameModule::~GameModule()
    {
        if (m_library.IsLoaded())
        {
            m_library.Unload();
        }
    }

    bool GameModule::LoadCopy(const std::filesystem::path& library, std::string& error)
    {
        std::error_code ec;
        std::filesystem::create_directories(m_shadowDirectory, ec);
        ++m_generation;
        std::filesystem::path copy = m_shadowDirectory / (library.stem().string() + "_" + std::to_string(m_generation) + library.extension().string());
        std::filesystem::copy_file(library, copy, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            error = "cannot copy module: " + ec.message();
            return false;
        }
        Reflection::TypeRegistry::SetCurrentModule(m_name);
        bool loaded = m_library.Load(copy);
        Reflection::TypeRegistry::SetCurrentModule("BrightBox3D");
        if (!loaded)
        {
            error = m_library.LastError();
            std::filesystem::remove(copy, ec);
            return false;
        }
        if (!m_loadedCopy.empty() && m_loadedCopy != copy)
        {
            std::filesystem::remove(m_loadedCopy, ec);
        }
        m_loadedCopy = copy;
        return true;
    }

    bool GameModule::Load(const std::filesystem::path& library, const std::string& name, const std::filesystem::path& shadowDirectory, std::string& error)
    {
        m_name = name;
        m_shadowDirectory = shadowDirectory;
        if (!std::filesystem::exists(library))
        {
            error = "module '" + library.generic_string() + "' does not exist";
            return false;
        }
        if (!LoadCopy(library, error))
        {
            return false;
        }
        size_t count = Reflection::TypeRegistry::GetFromModule(m_name).size();
        Core::Log::Info("Loaded game module '" + m_name + "' (" + std::to_string(count) + " reflected types)");
        return true;
    }

    void GameModule::Unload()
    {
        if (!m_library.IsLoaded())
        {
            return;
        }
        std::string name = m_name;
        Reflection::TypeRegistry::OnModuleUnloading().Broadcast(name);
        auto predicate = [name](const Reflection::TypeInfo& type) { return type.GetModuleName() == name; };
        std::vector<Level::ReplacedObject> records;
        Level::LevelObjectLibrary::Get().Clear();
        for (Level::Level* level : Level::Level::GetAllLevels())
        {
            Level::ObjectReplacer::Extract(*level, predicate, records);
        }
        Level::ObjectReplacer::DestroyExtracted(records);
        m_library.Unload();
        Level::ObjectReplacer::Restore(records);
    }

    bool GameModule::Reload(const std::filesystem::path& library, std::string& error)
    {
        if (!std::filesystem::exists(library))
        {
            error = "module '" + library.generic_string() + "' does not exist";
            return false;
        }
        std::string name = m_name;
        Reflection::TypeRegistry::OnModuleUnloading().Broadcast(name);
        auto predicate = [name](const Reflection::TypeInfo& type) { return type.GetModuleName() == name; };
        std::vector<Level::ReplacedObject> records;
        Level::LevelObjectLibrary::Get().Clear();
        for (Level::Level* level : Level::Level::GetAllLevels())
        {
            Level::ObjectReplacer::Extract(*level, predicate, records);
        }
        Level::ObjectReplacer::DestroyExtracted(records);

        std::filesystem::path previous = m_loadedCopy;
        m_library.Unload();
        bool loaded = LoadCopy(library, error);
        if (!loaded && !previous.empty())
        {
            std::string restoreError;
            Reflection::TypeRegistry::SetCurrentModule(m_name);
            m_library.Load(previous);
            Reflection::TypeRegistry::SetCurrentModule("BrightBox3D");
            m_loadedCopy = previous;
            Core::Log::Error("Hot reload failed to load the new module; the previous module was restored");
        }
        size_t restored = records.size();
        Level::ObjectReplacer::Restore(records);
        if (loaded)
        {
            Core::Log::Info("Hot reloaded '" + m_name + "' (generation " + std::to_string(m_generation) + ", " + std::to_string(restored) + " objects preserved)");
        }
        return loaded;
    }
}
