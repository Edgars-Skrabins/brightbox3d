#include "Reflection/TypeRedirects.h"

#include <mutex>
#include <unordered_map>

namespace BrightBox3D::Reflection
{
    namespace
    {
        struct RedirectState
        {
            std::mutex mutex;
            std::unordered_map<std::string, std::string> redirects;
        };

        RedirectState& State()
        {
            static RedirectState* state = new RedirectState();
            return *state;
        }
    }

    void TypeRedirects::Add(const std::string& oldName, const std::string& newName)
    {
        RedirectState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.redirects[oldName] = newName;
    }

    const std::string* TypeRedirects::Resolve(const std::string& oldName)
    {
        RedirectState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto it = state.redirects.find(oldName);
        return it == state.redirects.end() ? nullptr : &it->second;
    }

    TypeRedirectRegistration::TypeRedirectRegistration(const char* oldName, const char* newName)
    {
        TypeRedirects::Add(oldName, newName);
    }
}
