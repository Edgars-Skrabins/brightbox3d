#pragma once

#include "Core/Guid.h"
#include "Math/Vector2.h"
#include "UI/Binding.h"
#include "UI/Element.h"
#include "UI/Style.h"
#include "UI/StyleSheet.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace pugi
{
    class xml_node;
}

namespace BrightBox3D::RHI
{
    class Device;
}

namespace BrightBox3D::Rendering
{
    class GpuResourceCache;
}

namespace BrightBox3D::UI
{
    class DrawList;
    class WidgetController;

    struct UIInput
    {
        Math::Vector2 mouse{ -1.0f, -1.0f };
        bool mouseDown = false;
        bool mousePressed = false;
        bool mouseReleased = false;
        bool pointerEnabled = true;
        std::u32string text;
        bool backspace = false;
        bool enter = false;
        bool escape = false;
    };

    class Document
    {
    public:
        Document();
        ~Document();

        Document(const Document&) = delete;
        Document& operator=(const Document&) = delete;

        bool LoadScreen(const Core::Guid& screen, std::string& error);
        bool LoadFromText(const std::string& markup, std::string& error);
        void Update(const BindingRoots& roots, const UIInput& input, float width, float height, float deltaTime);
        void Draw(DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources) const;

        Element* GetRoot() const { return m_root.get(); }
        Element* Find(const std::string& name) const { return m_root ? m_root->FindByName(name) : nullptr; }
        const BindingRoots& GetRoots() const { return m_roots; }
        const std::unordered_set<Core::Guid>& GetDependencies() const { return m_dependencies; }
        bool IsPointerOverInteractive() const { return m_hovered != nullptr; }
        bool HasFocus() const { return m_focused != nullptr; }

    private:
        std::unique_ptr<Element> BuildElement(const pugi::xml_node& node, int depth, std::string& error);
        bool LoadStyleSheet(const std::string& reference);
        Core::Guid ResolveAsset(const std::string& reference, const char* expectedType);
        void ComputeStyles(Element& element, const ComputedStyle* parent);
        void SyncLayout(Element& element);
        void ReadLayout(Element& element, float parentX, float parentY);
        void UpdateBindings(Element& element);
        void HandleInput(const UIInput& input);
        Element* HitTest(Element& element, float x, float y);
        void DrawElement(const Element& element, DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources, float opacity) const;
        void ReportBindingError(const std::string& error);
        void WriteBinding(Element& element, const char* attribute);
        void NotifyValueChanged(Element& element);

        std::unique_ptr<Element> m_root;
        StyleSheet m_styles;
        std::vector<std::unique_ptr<WidgetController>> m_controllers;
        std::unordered_set<Core::Guid> m_dependencies;
        std::unordered_set<std::string> m_reportedErrors;
        BindingRoots m_roots;
        Element* m_hovered = nullptr;
        Element* m_pressed = nullptr;
        Element* m_focused = nullptr;
        bool m_created = false;
    };
}
