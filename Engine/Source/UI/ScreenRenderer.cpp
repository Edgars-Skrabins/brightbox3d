#include "UI/ScreenRenderer.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Math/Matrix4.h"
#include "Platform/Window.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/ShaderLibrary.h"
#include "Shaders/ShaderBindings.h"

#include <cstddef>

namespace BrightBox3D::UI
{
    ScreenRenderer& ScreenRenderer::Get()
    {
        static ScreenRenderer* renderer = new ScreenRenderer();
        return *renderer;
    }

    void ScreenRenderer::Initialize(RHI::Device& device, Rendering::ShaderLibrary& shaders, Rendering::GpuResourceCache& resources)
    {
        m_device = &device;
        m_shaders = &shaders;
        m_resources = &resources;
        RHI::SamplerDesc desc;
        desc.filter = RHI::Filter::Linear;
        desc.addressU = desc.addressV = desc.addressW = RHI::AddressMode::Clamp;
        m_sampler = device.CreateSampler(desc);
        m_assetHandle = Assets::AssetDatabase::Get().OnAssetChanged.Subscribe([this](const Core::Guid& guid)
        {
            for (auto& [camera, entry] : m_entries)
            {
                if (entry.document && entry.document->GetDependencies().count(guid))
                {
                    entry.document.reset();
                    entry.failed = false;
                }
            }
        });
        m_moduleHandle = Reflection::TypeRegistry::OnModuleUnloading().Subscribe([this](const std::string&) { ClearDocuments(); });
    }

    void ScreenRenderer::Shutdown()
    {
        if (!m_device)
        {
            return;
        }
        ClearDocuments();
        Assets::AssetDatabase::Get().OnAssetChanged.Unsubscribe(m_assetHandle);
        Reflection::TypeRegistry::OnModuleUnloading().Unsubscribe(m_moduleHandle);
        m_sampler.reset();
        m_device = nullptr;
        m_shaders = nullptr;
        m_resources = nullptr;
    }

    void ScreenRenderer::ClearDocuments()
    {
        m_entries.clear();
    }

    Document* ScreenRenderer::GetDocument(const Core::Guid& camera) const
    {
        auto it = m_entries.find(camera);
        return it == m_entries.end() ? nullptr : it->second.document.get();
    }

    void ScreenRenderer::RenderScreen(RHI::CommandList& commands, const Core::Guid& view, const Core::Guid& screen, const BindingRoots& roots, RHI::Texture* target, Platform::Window* window)
    {
        m_pointerOverUI = false;
        if (!m_device || !target)
        {
            return;
        }
        Entry& entry = m_entries[view];
        if (!screen.IsValid())
        {
            entry.document.reset();
            entry.screen = {};
            return;
        }
        if (entry.screen != screen)
        {
            entry.screen = screen;
            entry.document.reset();
            entry.failed = false;
        }
        if (!entry.document && !entry.failed)
        {
            auto document = std::make_unique<Document>();
            std::string error;
            if (document->LoadScreen(screen, error))
            {
                entry.document = std::move(document);
            }
            else
            {
                entry.failed = true;
                Core::Log::Error(Core::Text(Core::EngineText::UIScreenLoadFailed, error));
            }
        }
        if (!entry.document)
        {
            return;
        }

        float scale = 1.0f;
        UIInput input;
        float width = static_cast<float>(target->GetWidth());
        float height = static_cast<float>(target->GetHeight());
        if (window)
        {
            Math::Vector2 size = window->GetSize();
            if (size.x > 0.0f)
            {
                scale = static_cast<float>(target->GetWidth()) / size.x;
            }
            width = size.x;
            height = size.y;
            const Platform::InputState& state = window->GetInput();
            input.pointerEnabled = window->GetCursorMode() == Platform::CursorMode::Normal;
            input.mouse = state.GetMousePosition();
            input.mouseDown = state.IsMouseButtonDown(Platform::MouseButton::Left);
            input.mousePressed = state.WasMouseButtonPressed(Platform::MouseButton::Left);
            input.mouseReleased = state.WasMouseButtonReleased(Platform::MouseButton::Left);
            input.text = state.GetTextInput();
            input.backspace = state.WasKeyPressed(Platform::Key::Backspace);
            input.enter = state.WasKeyPressed(Platform::Key::Enter) || state.WasKeyPressed(Platform::Key::KeypadEnter);
            input.escape = state.WasKeyPressed(Platform::Key::Escape);
        }

        double now = Platform::Window::GetTime();
        float deltaTime = m_lastTime > 0.0 ? static_cast<float>(std::min(now - m_lastTime, 0.1)) : 0.0f;
        m_lastTime = now;
        entry.document->Update(roots, input, width, height, deltaTime);
        m_pointerOverUI = entry.document->IsPointerOverInteractive() || entry.document->HasFocus();
        RenderDocument(commands, *entry.document, target, scale);
    }

    void ScreenRenderer::RenderDocument(RHI::CommandList& commands, Document& document, RHI::Texture* target, float scale)
    {
        m_drawList.Clear();
        document.Draw(m_drawList, *m_device, *m_resources);
        if (m_drawList.GetIndices().empty())
        {
            return;
        }
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders->Get(Rendering::ShaderProgram::UI, RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders->Get(Rendering::ShaderProgram::UI, RHI::ShaderStage::Pixel);
        desc.vertexLayout.attributes = {
            { "POSITION", 0, RHI::Format::RG32Float, offsetof(UIVertex, position), 0 },
            { "TEXCOORD", 0, RHI::Format::RG32Float, offsetof(UIVertex, uv), 0 },
            { "TEXCOORD", 1, RHI::Format::RG32Float, offsetof(UIVertex, local), 0 },
            { "TEXCOORD", 2, RHI::Format::RG32Float, offsetof(UIVertex, halfSize), 0 },
            { "TEXCOORD", 3, RHI::Format::RGBA32Float, offsetof(UIVertex, params), 0 },
            { "COLOR", 0, RHI::Format::RGBA8Unorm, offsetof(UIVertex, color), 0 }
        };
        desc.vertexLayout.strides[0] = sizeof(UIVertex);
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.rasterizer.scissor = true;
        desc.depthStencil.depthTest = false;
        desc.depthStencil.depthWrite = false;
        desc.colorFormats = { target->GetFormat() };
        desc.blend[0] = RHI::BlendState::AlphaBlend();
        RHI::Pipeline* pipeline = m_shaders->GetPipeline({ Rendering::ShaderProgram::UI, target->GetFormat() }, desc);
        if (!pipeline)
        {
            return;
        }

        float logicalWidth = static_cast<float>(target->GetWidth()) / scale;
        float logicalHeight = static_cast<float>(target->GetHeight()) / scale;
        Math::Matrix4 projection = Math::Matrix4::OrthographicOffCenterLH(0.0f, logicalWidth, logicalHeight, 0.0f, 0.0f, 1.0f);

        RHI::RenderPassDesc pass;
        pass.colors.push_back({ target, 0, 0, RHI::LoadAction::Load });
        pass.debugName = "UI";
        commands.BeginRenderPass(pass);
        commands.SetPipeline(pipeline);
        commands.SetConstants(BRIGHT_SLOT_LOCAL_CONSTANTS, projection);
        commands.SetSampler(BRIGHT_SLOT_OVERLAY_SAMPLER, m_sampler.get());
        commands.SetVertexData(0, m_drawList.GetVertices().data(), m_drawList.GetVertices().size() * sizeof(UIVertex));
        commands.SetIndexData(m_drawList.GetIndices().data(), m_drawList.GetIndices().size() * sizeof(uint32_t), RHI::IndexFormat::UInt32);
        for (const UIBatch& batch : m_drawList.GetBatches())
        {
            if (batch.indexCount == 0)
            {
                continue;
            }
            RHI::ScissorRect scissor{ 0, 0, static_cast<int32_t>(target->GetWidth()), static_cast<int32_t>(target->GetHeight()) };
            if (batch.clipped)
            {
                scissor.x = static_cast<int32_t>(batch.clip[0] * scale);
                scissor.y = static_cast<int32_t>(batch.clip[1] * scale);
                scissor.width = std::max(0, static_cast<int32_t>((batch.clip[2] - batch.clip[0]) * scale));
                scissor.height = std::max(0, static_cast<int32_t>((batch.clip[3] - batch.clip[1]) * scale));
            }
            commands.SetScissor(scissor);
            commands.SetTexture(BRIGHT_SLOT_OVERLAY_TEXTURE, batch.texture ? batch.texture : m_resources->GetWhiteTexture());
            commands.DrawIndexed(batch.indexCount, 1, batch.firstIndex, 0, 0);
        }
        commands.EndRenderPass();
    }
}
