#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace BrightBox3D::Core
{
    using EventHandle = uint64_t;

    template <typename... Args>
    class Event
    {
    public:
        EventHandle Subscribe(std::function<void(Args...)> callback)
        {
            EventHandle handle = ++m_nextHandle;
            m_listeners.push_back({ handle, std::move(callback) });
            return handle;
        }

        void Unsubscribe(EventHandle handle)
        {
            for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
            {
                if (it->handle == handle)
                {
                    m_listeners.erase(it);
                    return;
                }
            }
        }

        void Broadcast(Args... args) const
        {
            auto listeners = m_listeners;
            for (const Listener& listener : listeners)
            {
                listener.callback(args...);
            }
        }

        void Clear() { m_listeners.clear(); }
        bool IsEmpty() const { return m_listeners.empty(); }

    private:
        struct Listener
        {
            EventHandle handle;
            std::function<void(Args...)> callback;
        };

        std::vector<Listener> m_listeners;
        EventHandle m_nextHandle = 0;
    };
}
