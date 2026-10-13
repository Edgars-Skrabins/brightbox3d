#include "ImGui/ImGuiRenderer.h"

#include "Math/Matrix4.h"
#include "Shaders/ShaderBindings.h"

namespace BrightBox3D::Editor
{
    ImGuiRenderer::ImGuiRenderer(RHI::Device& device, Rendering::ShaderLibrary& shaders) : m_device(device), m_shaders(shaders)
    {
        RHI::SamplerDesc desc;
        desc.filter = RHI::Filter::Linear;
        desc.mipLinear = false;
        desc.addressU = desc.addressV = desc.addressW = RHI::AddressMode::Clamp;
        m_sampler = m_device.CreateSampler(desc);
        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererName = "BrightBox3D RHI";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    }

    ImGuiRenderer::~ImGuiRenderer()
    {
        m_device.WaitIdle();
        for (ImTextureData* texture : ImGui::GetPlatformIO().Textures)
        {
            texture->SetTexID(ImTextureID_Invalid);
            texture->SetStatus(ImTextureStatus_Destroyed);
        }
    }

    void ImGuiRenderer::UpdateTexture(ImTextureData* texture)
    {
        if (texture->Status == ImTextureStatus_WantCreate || texture->Status == ImTextureStatus_WantUpdates)
        {
            auto it = m_textures.find(texture);
            if (it == m_textures.end() || it->second->GetWidth() != static_cast<uint32_t>(texture->Width) || it->second->GetHeight() != static_cast<uint32_t>(texture->Height))
            {
                RHI::TextureDesc desc;
                desc.width = static_cast<uint32_t>(texture->Width);
                desc.height = static_cast<uint32_t>(texture->Height);
                desc.format = RHI::Format::RGBA8Unorm;
                desc.debugName = "ImGuiTexture";
                m_textures[texture] = m_device.CreateTexture(desc);
            }
            RHI::Texture* target = m_textures[texture].get();
            m_device.UpdateTexture(target, 0, 0, texture->GetPixels(), static_cast<uint32_t>(texture->GetPitch()));
            texture->SetTexID(ToTextureId(target));
            texture->SetStatus(ImTextureStatus_OK);
        }
        else if (texture->Status == ImTextureStatus_WantDestroy && texture->UnusedFrames > 2)
        {
            m_textures.erase(texture);
            texture->SetTexID(ImTextureID_Invalid);
            texture->SetStatus(ImTextureStatus_Destroyed);
        }
    }

    void ImGuiRenderer::Render(RHI::CommandList& commands, ImDrawData* drawData, RHI::Texture* target)
    {
        if (drawData->Textures)
        {
            for (ImTextureData* texture : *drawData->Textures)
            {
                if (texture->Status != ImTextureStatus_OK)
                {
                    UpdateTexture(texture);
                }
            }
        }

        RHI::RenderPassDesc pass;
        pass.colors.push_back({ target, 0, 0, RHI::LoadAction::Clear, { 0.11f, 0.11f, 0.12f, 1.0f } });
        pass.debugName = "ImGui";
        commands.BeginRenderPass(pass);

        float framebufferWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
        float framebufferHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;
        if (framebufferWidth <= 0.0f || framebufferHeight <= 0.0f || drawData->TotalVtxCount == 0)
        {
            commands.EndRenderPass();
            return;
        }

        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get(Rendering::ShaderProgram::ImGui, RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get(Rendering::ShaderProgram::ImGui, RHI::ShaderStage::Pixel);
        desc.vertexLayout.attributes = {
            { "POSITION", 0, RHI::Format::RG32Float, offsetof(ImDrawVert, pos), 0 },
            { "TEXCOORD", 0, RHI::Format::RG32Float, offsetof(ImDrawVert, uv), 0 },
            { "COLOR", 0, RHI::Format::RGBA8Unorm, offsetof(ImDrawVert, col), 0 }
        };
        desc.vertexLayout.strides[0] = sizeof(ImDrawVert);
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.rasterizer.scissor = true;
        desc.depthStencil.depthTest = false;
        desc.depthStencil.depthWrite = false;
        desc.colorFormats = { target->GetFormat() };
        desc.blend[0] = RHI::BlendState::AlphaBlend();
        RHI::Pipeline* pipeline = m_shaders.GetPipeline({ Rendering::ShaderProgram::ImGui, target->GetFormat() }, desc);
        if (!pipeline)
        {
            commands.EndRenderPass();
            return;
        }

        commands.SetPipeline(pipeline);
        RHI::Viewport viewport;
        viewport.width = framebufferWidth;
        viewport.height = framebufferHeight;
        commands.SetViewport(viewport);

        float left = drawData->DisplayPos.x;
        float right = drawData->DisplayPos.x + drawData->DisplaySize.x;
        float top = drawData->DisplayPos.y;
        float bottom = drawData->DisplayPos.y + drawData->DisplaySize.y;
        Math::Matrix4 projection;
        projection.m[0][0] = 2.0f / (right - left);
        projection.m[1][1] = 2.0f / (top - bottom);
        projection.m[2][2] = 0.5f;
        projection.m[3][0] = (right + left) / (left - right);
        projection.m[3][1] = (top + bottom) / (bottom - top);
        projection.m[3][2] = 0.5f;
        commands.SetConstants(BRIGHT_SLOT_LOCAL_CONSTANTS, projection);
        commands.SetSampler(BRIGHT_SLOT_OVERLAY_SAMPLER, m_sampler.get());

        ImVec2 clipOffset = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;
        for (const ImDrawList* list : drawData->CmdLists)
        {
            commands.SetVertexData(0, list->VtxBuffer.Data, static_cast<size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert));
            commands.SetIndexData(list->IdxBuffer.Data, static_cast<size_t>(list->IdxBuffer.Size) * sizeof(ImDrawIdx), sizeof(ImDrawIdx) == 2 ? RHI::IndexFormat::UInt16 : RHI::IndexFormat::UInt32);
            for (const ImDrawCmd& command : list->CmdBuffer)
            {
                if (command.UserCallback)
                {
                    if (command.UserCallback != ImDrawCallback_ResetRenderState)
                    {
                        command.UserCallback(list, &command);
                    }
                    commands.SetPipeline(pipeline);
                    continue;
                }
                ImVec2 clipMin((command.ClipRect.x - clipOffset.x) * clipScale.x, (command.ClipRect.y - clipOffset.y) * clipScale.y);
                ImVec2 clipMax((command.ClipRect.z - clipOffset.x) * clipScale.x, (command.ClipRect.w - clipOffset.y) * clipScale.y);
                clipMin.x = std::max(clipMin.x, 0.0f);
                clipMin.y = std::max(clipMin.y, 0.0f);
                clipMax.x = std::min(clipMax.x, framebufferWidth);
                clipMax.y = std::min(clipMax.y, framebufferHeight);
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }
                RHI::ScissorRect scissor{ static_cast<int32_t>(clipMin.x), static_cast<int32_t>(clipMin.y), static_cast<int32_t>(clipMax.x - clipMin.x), static_cast<int32_t>(clipMax.y - clipMin.y) };
                commands.SetScissor(scissor);
                auto* texture = reinterpret_cast<RHI::Texture*>(command.GetTexID());
                commands.SetTexture(BRIGHT_SLOT_OVERLAY_TEXTURE, texture);
                commands.DrawIndexed(command.ElemCount, 1, command.IdxOffset, static_cast<int32_t>(command.VtxOffset), 0);
            }
        }
        commands.EndRenderPass();
    }
}
