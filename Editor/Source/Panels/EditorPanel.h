#pragma once

#include "EditorWindows.h"
#include "Serialization/Json.h"

#include <imgui.h>

#include <string>

namespace BrightBox3D::Editor
{
    class EditorPanel
    {
    public:
        EditorPanel(EditorWindow type, int instanceIndex) : m_type(type), m_instanceIndex(instanceIndex) {}
        virtual ~EditorPanel() = default;

        virtual void Draw() = 0;
        virtual void SaveState(Serialization::JsonValue& state, Serialization::JsonAllocator& allocator) const {}
        virtual void LoadState(const Serialization::JsonValue& state) {}

        EditorWindow GetWindowType() const { return m_type; }
        int GetInstanceIndex() const { return m_instanceIndex; }
        bool IsOpen() const { return m_open; }
        void Close() { m_open = false; }
        void RequestFocus() { m_focusRequested = true; }
        std::string GetWindowName() const;

    protected:
        bool BeginWindow(ImGuiWindowFlags flags = ImGuiWindowFlags_None);

    private:
        EditorWindow m_type;
        int m_instanceIndex = 1;
        bool m_open = true;
        bool m_focusRequested = false;
    };
}
