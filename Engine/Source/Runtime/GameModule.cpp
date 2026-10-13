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
        bool loaded = m_library.Load(copy);
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

    void GameModule::ExtractModuleObjects(Level::ReplacementBatch& batch)
    {
        std::string name = m_name;
        Reflection::TypeRegistry::OnModuleUnloading().Broadcast(name);
        auto predicate = [name](const Reflection::TypeInfo& type) { return type.GetModuleName() == name; };
        Level::LevelObjectLibrary::Get().Clear();
        for (Level::Level* level : Level::Level::GetAllLevels())
        {
            Level::ObjectReplacer::Extract(*level, predicate, batch);
        }
        Level::ObjectReplacer::DestroyExtracted(batch);
    }

    void GameModule::RestorePreviousModule(const std::filesystem::path& previous)
    {
        bool restored = m_library.Load(previous);
        if (restored)
        {
            m_loadedCopy = previous;
            Core::Log::Error("Hot reload failed to load the new module; the previous module was restored");
            return;
        }
        Core::Log::Error("Hot reload failed to load the new module, and the previous module could not be restored: " + m_library.LastError());
        std::error_code ec;
        std::filesystem::remove(previous, ec);
        m_loadedCopy.clear();
    }

    void GameModule::Unload()
    {
        if (!m_library.IsLoaded())
        {
            return;
        }
        Level::ReplacementBatch batch;
        ExtractModuleObjects(batch);
        m_library.Unload();
        Level::ObjectReplacer::Restore(batch);
    }

    bool GameModule::Reload(const std::filesystem::path& library, std::string& error)
    {
        if (!std::filesystem::exists(library))
        {
            error = "module '" + library.generic_string() + "' does not exist";
            return false;
        }
        Level::ReplacementBatch batch;
        ExtractModuleObjects(batch);
        std::filesystem::path previous = m_loadedCopy;
        m_library.Unload();
        bool loaded = LoadCopy(library, error);
        if (!loaded && !previous.empty())
        {
            RestorePreviousModule(previous);
        }
        size_t restored = batch.Count();
        Level::ObjectReplacer::Restore(batch);
        if (loaded)
        {
            Core::Log::Info("Hot reloaded '" + m_name + "' (generation " + std::to_string(m_generation) + ", " + std::to_string(restored) + " objects preserved)");
        }
        return loaded;
    }
}
