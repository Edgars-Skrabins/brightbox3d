#pragma once

#include "Core/Guid.h"
#include "Core/Result.h"
#include "UI/Binding.h"
#include "UI/BindingUpdater.h"
#include "UI/DocumentInput.h"
#include "UI/Element.h"
#include "UI/StyleSheet.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

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

    class Document
    {
    public:
        Document();
        ~Document();

        Document(const Document&) = delete;
        Document& operator=(const Document&) = delete;

        Core::Result<void> LoadScreen(const Core::Guid& screen);
        Core::Result<void> LoadFromText(const std::string& markup);
        void Update(const BindingRoots& roots, const UIInput& input, float width, float height, float deltaTime);
        void Draw(DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources) const;

        Element* GetRoot() const { return m_root.get(); }
        Element* Find(const std::string& name) const { return m_root ? m_root->FindByName(name) : nullptr; }
        const BindingRoots& GetRoots() const { return m_bindings.GetRoots(); }
        const std::unordered_set<Core::Guid>& GetDependencies() const { return m_dependencies; }
        bool IsPointerOverInteractive() const { return m_input.IsPointerOverInteractive(); }
        bool HasFocus() const { return m_input.HasFocus(); }

    private:
        std::unique_ptr<Element> m_root;
        StyleSheet m_styles;
        std::vector<std::unique_ptr<WidgetController>> m_controllers;
        std::unordered_set<Core::Guid> m_dependencies;
        BindingUpdater m_bindings;
        DocumentInput m_input;
        bool m_created = false;
    };
}
