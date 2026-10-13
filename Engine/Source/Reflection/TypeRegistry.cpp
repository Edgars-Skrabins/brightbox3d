#include "Reflection/TypeRegistry.h"

#include "Core/Log.h"
#include "Reflection/TypeRedirects.h"

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
        if (it != state.types.end())
        {
            return it->second;
        }
        const std::string* redirect = TypeRedirects::Resolve(name);
        if (!redirect)
        {
            return nullptr;
        }
        it = state.types.find(*redirect);
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

    Core::Event<const std::string&>& TypeRegistry::OnModuleUnloading()
    {
        static Core::Event<const std::string&>* event = new Core::Event<const std::string&>();
        return *event;
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
