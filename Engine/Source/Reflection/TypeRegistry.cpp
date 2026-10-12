#include "Reflection/TypeRegistry.h"

#include "Core/Log.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace BrightBox3D::Reflection
{
    namespace
    {
        struct RegistryState
        {
            std::recursive_mutex mutex;
            std::unordered_map<std::string, const TypeInfo*> types;
            std::unordered_map<std::string, const EnumInfo*> enums;
            std::string currentModule = "BrightBox3D";
            uint64_t revision = 0;
        };

        RegistryState& State()
        {
            static RegistryState* state = new RegistryState();
            return *state;
        }
    }

    void TypeRegistry::Register(const TypeInfo& type)
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        auto it = state.types.find(type.GetName());
        if (it != state.types.end() && it->second != &type)
        {
            Core::Log::Warning("Reflected type '" + type.GetName() + "' is registered twice; the newest registration wins");
        }
        const_cast<TypeInfo&>(type).SetModuleName(state.currentModule);
        state.types[type.GetName()] = &type;
        state.revision++;
    }

    void TypeRegistry::Unregister(const TypeInfo& type)
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        auto it = state.types.find(type.GetName());
        if (it != state.types.end() && it->second == &type)
        {
            state.types.erase(it);
            state.revision++;
        }
    }

    void TypeRegistry::RegisterEnum(const EnumInfo& info)
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        state.enums[info.name] = &info;
        state.revision++;
    }

    void TypeRegistry::UnregisterEnum(const EnumInfo& info)
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        auto it = state.enums.find(info.name);
        if (it != state.enums.end() && it->second == &info)
        {
            state.enums.erase(it);
            state.revision++;
        }
    }

    const TypeInfo* TypeRegistry::Find(const std::string& name)
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        auto it = state.types.find(name);
        return it == state.types.end() ? nullptr : it->second;
    }

    const EnumInfo* TypeRegistry::FindEnum(const std::string& name)
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        auto it = state.enums.find(name);
        return it == state.enums.end() ? nullptr : it->second;
    }

    std::vector<const TypeInfo*> TypeRegistry::GetAll()
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        std::vector<const TypeInfo*> result;
        for (const auto& [name, type] : state.types)
        {
            result.push_back(type);
        }
        std::sort(result.begin(), result.end(), [](const TypeInfo* a, const TypeInfo* b) { return a->GetName() < b->GetName(); });
        return result;
    }

    std::vector<const TypeInfo*> TypeRegistry::GetDerived(const TypeInfo& base, bool includeAbstract)
    {
        std::vector<const TypeInfo*> result;
        for (const TypeInfo* type : GetAll())
        {
            if (type->IsA(base) && (includeAbstract || !type->IsAbstract()))
            {
                result.push_back(type);
            }
        }
        return result;
    }

    std::vector<const TypeInfo*> TypeRegistry::GetFromModule(const std::string& moduleName)
    {
        std::vector<const TypeInfo*> result;
        for (const TypeInfo* type : GetAll())
        {
            if (type->GetModuleName() == moduleName)
            {
                result.push_back(type);
            }
        }
        return result;
    }

    uint64_t TypeRegistry::Revision()
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        return state.revision;
    }

    void TypeRegistry::SetCurrentModule(const std::string& moduleName)
    {
        RegistryState& state = State();
        std::lock_guard<std::recursive_mutex> lock(state.mutex);
        state.currentModule = moduleName;
    }

    const std::string& TypeRegistry::GetCurrentModule()
    {
        return State().currentModule;
    }

    TypeRegistration::TypeRegistration(Getter getter) : m_type(&getter())
    {
        TypeRegistry::Register(*m_type);
    }

    TypeRegistration::~TypeRegistration()
    {
        TypeRegistry::Unregister(*m_type);
    }

    EnumRegistration::EnumRegistration(const EnumInfo& info) : m_info(&info)
    {
        TypeRegistry::RegisterEnum(info);
    }

    EnumRegistration::~EnumRegistration()
    {
        TypeRegistry::UnregisterEnum(*m_info);
    }
}
