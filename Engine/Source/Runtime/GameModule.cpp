#include "Runtime/GameModule.h"

#include "Core/EngineText.h"
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

    Core::Result<void> GameModule::LoadCopy(const std::filesystem::path& library)
    {
        std::error_code ec;
        std::filesystem::create_directories(m_shadowDirectory, ec);
        ++m_generation;
        std::filesystem::path copy = m_shadowDirectory / (library.stem().string() + "_" + std::to_string(m_generation) + library.extension().string());
        std::filesystem::copy_file(library, copy, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            return Core::Error(Core::EngineText::ModuleCopyFailed, ec.message());
        }
        if (Core::Result<void> loaded = m_library.Load(copy); !loaded)
        {
            std::filesystem::remove(copy, ec);
            return loaded;
        }
        if (!m_loadedCopy.empty() && m_loadedCopy != copy)
        {
            std::filesystem::remove(m_loadedCopy, ec);
        }
        m_loadedCopy = copy;
        return {};
    }

    Core::Result<void> GameModule::Load(const std::filesystem::path& library, const std::string& name, const std::filesystem::path& shadowDirectory)
    {
        m_name = name;
        m_shadowDirectory = shadowDirectory;
        if (!std::filesystem::exists(library))
        {
            return Core::Error(Core::EngineText::ModuleMissing, library.generic_string());
        }
        if (Core::Result<void> loaded = LoadCopy(library); !loaded)
        {
            return loaded;
        }
        size_t count = Reflection::TypeRegistry::GetFromModule(m_name).size();
        Core::Log::Info(Core::Text(Core::EngineText::GameModuleLoaded, m_name, count));
        return {};
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
        Core::Result<void> restored = m_library.Load(previous);
        if (restored)
        {
            m_loadedCopy = previous;
            Core::Log::Error(Core::Text(Core::EngineText::ModuleRestored));
            return;
        }
        Core::Log::Error(Core::Text(Core::EngineText::ModuleRestoreFailed, restored.GetError().GetText()));
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

    Core::Result<void> GameModule::Reload(const std::filesystem::path& library)
    {
        if (!std::filesystem::exists(library))
        {
            return Core::Error(Core::EngineText::ModuleMissing, library.generic_string());
        }
        Level::ReplacementBatch batch;
        ExtractModuleObjects(batch);
        std::filesystem::path previous = m_loadedCopy;
        m_library.Unload();
        Core::Result<void> loaded = LoadCopy(library);
        if (!loaded && !previous.empty())
        {
            RestorePreviousModule(previous);
        }
        size_t restored = batch.Count();
        Level::ObjectReplacer::Restore(batch);
        if (loaded)
        {
            Core::Log::Info(Core::Text(Core::EngineText::GameModuleReloaded, m_name, m_generation, restored));
        }
        return loaded;
    }
}
