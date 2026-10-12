#pragma once

#include "RHI/Device.h"
#include "Rendering/ShaderLibrary.h"

#include <imgui.h>

#include <memory>
#include <unordered_map>

namespace BrightBox3D::Editor
{
    class ImGuiRenderer
    {
    public:
        ImGuiRenderer(RHI::Device& device, Rendering::ShaderLibrary& shaders);
        ~ImGuiRenderer();

        void Render(RHI::CommandList& commands, ImDrawData* drawData, RHI::Texture* target);

        static ImTextureID ToTextureId(RHI::Texture* texture) { return reinterpret_cast<ImTextureID>(texture); }

    private:
        void UpdateTexture(ImTextureData* texture);

        RHI::Device& m_device;
        Rendering::ShaderLibrary& m_shaders;
        std::unique_ptr<RHI::Sampler> m_sampler;
        std::unordered_map<ImTextureData*, std::unique_ptr<RHI::Texture>> m_textures;
    };
}
