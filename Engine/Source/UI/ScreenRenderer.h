#pragma once

#include "Core/Event.h"
#include "Core/Guid.h"
#include "RHI/Device.h"
#include "UI/Binding.h"
#include "UI/Document.h"
#include "UI/DrawList.h"

#include <memory>
#include <unordered_map>

namespace BrightBox3D::Platform
{
    class Window;
}

namespace BrightBox3D::Rendering
{
    class GpuResourceCache;
    class ShaderLibrary;
}


namespace BrightBox3D::UI
{
    class ScreenRenderer
    {
    public:
        static ScreenRenderer& Get();

        void Initialize(RHI::Device& device, Rendering::ShaderLibrary& shaders, Rendering::GpuResourceCache& resources);
        void Shutdown();
        bool IsInitialized() const { return m_device != nullptr; }

        void RenderScreen(RHI::CommandList& commands, const Core::Guid& view, const Core::Guid& screen, const BindingRoots& roots, RHI::Texture* target, Platform::Window* window);
        void RenderDocument(RHI::CommandList& commands, Document& document, RHI::Texture* target, float scale);
        Document* GetDocument(const Core::Guid& camera) const;
        void ClearDocuments();
        bool IsPointerOverUI() const { return m_pointerOverUI; }

    private:
        struct Entry
        {
            Core::Guid screen;
            std::unique_ptr<Document> document;
            bool failed = false;
        };

        RHI::Device* m_device = nullptr;
        Rendering::ShaderLibrary* m_shaders = nullptr;
        Rendering::GpuResourceCache* m_resources = nullptr;
        std::unique_ptr<RHI::Sampler> m_sampler;
        std::unordered_map<Core::Guid, Entry> m_entries;
        DrawList m_drawList;
        Core::EventHandle m_assetHandle = 0;
        Core::EventHandle m_moduleHandle = 0;
        double m_lastTime = 0.0;
        bool m_pointerOverUI = false;
    };
}
