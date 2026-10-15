#include "Level/LevelSubsystem.h"

#include <algorithm>
#include <mutex>

namespace BrightBox3D::Level
{
    namespace
    {
        struct RegistryState
        {
            std::mutex mutex;
            std::vector<std::pair<std::string, LevelSubsystemRegistry::Factory>> factories;
        };

        RegistryState& State()
        {
            static RegistryState* state = new RegistryState();
            return *state;
        }
    }

    void LevelSubsystemRegistry::Register(const Reflection::TypeInfo& type, Factory factory)
    {
        const std::string& name = type.GetName();
        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        for (auto& entry : state.factories)
        {
            if (entry.first == name)
            {
                entry.second = std::move(factory);
                return;
            }
        }
        state.factories.emplace_back(name, std::move(factory));
    }

    void LevelSubsystemRegistry::Unregister(const Reflection::TypeInfo& type)
    {
        const std::string& name = type.GetName();
        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.factories.erase(std::remove_if(state.factories.begin(), state.factories.end(), [&](const auto& entry) { return entry.first == name; }), state.factories.end());
    }

    std::vector<std::pair<std::string, LevelSubsystemRegistry::Factory>> LevelSubsystemRegistry::GetAll()
    {
        RegistryState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        return state.factories;
    }
}
