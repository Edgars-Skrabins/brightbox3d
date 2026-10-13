#include "Rendering/Renderer.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "Level/Level.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/DebugDraw.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/ModelAsset.h"

#include <AreaTex.h>
#include <SearchTex.h>

#include <algorithm>
#include <cstring>
#include <chrono>
#include <random>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint32_t MaxLights = 128;
        constexpr uint32_t CascadeCount = 4;

        const RHI::Format AlbedoFormat = RHI::Format::RGBA8Srgb;
        const RHI::Format NormalFormat = RHI::Format::RGBA16Float;
        const RHI::Format MaterialFormat = RHI::Format::RGBA8Unorm;
        const RHI::Format EmissionFormat = RHI::Format::RGBA16Float;
        const RHI::Format VelocityFormat = RHI::Format::RG16Float;
        const RHI::Format DepthCopyFormat = RHI::Format::R32Float;
        const RHI::Format SceneColorFormat = RHI::Format::RGBA16Float;
        const RHI::Format DepthFormat = RHI::Format::Depth32Float;

        struct ViewConstants
        {
            Math::Matrix4 view;
            Math::Matrix4 projection;
            Math::Matrix4 viewProjection;
            Math::Matrix4 inverseViewProjection;
            Math::Matrix4 unjitteredViewProjection;
            Math::Matrix4 previousViewProjection;
            float cameraPosition[4];
            float screenSize[4];
            float clipPlanes[4];
            float skyTop[4];
            float skyHorizon[4];
            float skyGround[4];
            float viewFlags[4];
        };

        struct ObjectConstants
        {
            Math::Matrix4 world;
            Math::Matrix4 previousWorld;
            Math::Matrix4 normal;
            float params[4];
        };

        struct MaterialConstants
        {
            float baseColor[4];
            float emissive[4];
            float surface[4];
            float uvTransform[4];
            float textureFlags[4];
            float materialFlags[4];
        };

        struct GpuLight
        {
            float positionRange[4];
            float directionType[4];
            float colorIntensity[4];
            float spotShadow[4];
        };

        struct LightConstants
        {
            float counts[4];
            float ambient[4];
            GpuLight lights[MaxLights];
        };

        struct ShadowConstants
        {
            Math::Matrix4 cascades[CascadeCount];
            float splits[4];
            float texelSizes[4];
            float params[4];
        };

        struct TonemapConstants
        {
            float exposure[4];
            float bloom[4];
            float grading[4];
            float tint[4];
            float vignette[4];
            float vignetteColor[4];
            float lift[4];
            float gamma[4];
            float gain[4];
        };

        struct LineConstants
        {
            float params[4];
        };

        struct PostConstants
        {
            float params[4];
            float params2[4];
            float sourceSize[4];
        };

        struct SSAOConstants
        {
            float params[4];
            float kernel[16][4];
        };

        float Halton(uint32_t index, uint32_t base)
        {
            float result = 0.0f;
            float fraction = 1.0f / static_cast<float>(base);
            while (index > 0)
            {
                result += fraction * static_cast<float>(index % base);
                index /= base;
                fraction /= static_cast<float>(base);
            }
            return result;
        }

        constexpr uint32_t LuminanceSize = 64;

        void Store(float* target, float x, float y, float z, float w)
        {
            target[0] = x;
            target[1] = y;
            target[2] = z;
            target[3] = w;
        }

        void StoreColor(float* target, const Math::Color& color, float w)
        {
            Store(target, color.r, color.g, color.b, w);
        }

        RHI::VertexLayout MeshLayout()
        {
            RHI::VertexLayout layout;
            layout.attributes = {
                { "POSITION", 0, RHI::Format::RGB32Float, 0, 0 },
                { "NORMAL", 0, RHI::Format::RGB32Float, 12, 0 },
                { "TANGENT", 0, RHI::Format::RGBA32Float, 24, 0 },
                { "TEXCOORD", 0, RHI::Format::RG32Float, 40, 0 }
            };
            layout.strides[0] = sizeof(MeshVertex);
            return layout;
        }

        RHI::VertexLayout LineLayout()
        {
            RHI::VertexLayout layout;
            layout.attributes = {
                { "POSITION", 0, RHI::Format::RGB32Float, 0, 0 },
                { "COLOR", 0, RHI::Format::RGBA8Unorm, 12, 0 }
            };
            layout.strides[0] = sizeof(LineVertex);
            return layout;
        }

        std::string FormatKey(RHI::Format format)
        {
            return std::to_string(static_cast<int>(format));
        }
    }

    struct Renderer::ViewTargets
    {
        uint32_t width = 0;
        uint32_t height = 0;
        std::unique_ptr<RHI::Texture> albedo;
        std::unique_ptr<RHI::Texture> normal;
        std::unique_ptr<RHI::Texture> material;
        std::unique_ptr<RHI::Texture> emission;
        std::unique_ptr<RHI::Texture> velocity;
        std::unique_ptr<RHI::Texture> depthCopy;
        std::unique_ptr<RHI::Texture> depth;
        std::unique_ptr<RHI::Texture> sceneColor;
        std::unique_ptr<RHI::Texture> ao;
        std::unique_ptr<RHI::Texture> aoBlur;
        std::vector<std::unique_ptr<RHI::Texture>> bloom;
        std::unique_ptr<RHI::Texture> luminance;
        std::unique_ptr<RHI::Texture> exposure[2];
        std::unique_ptr<RHI::Texture> history[2];
        std::unique_ptr<RHI::Texture> ldr;
        std::unique_ptr<RHI::Texture> smaaEdges;
        std::unique_ptr<RHI::Texture> smaaBlend;
        uint32_t historyIndex = 0;
        uint32_t exposureIndex = 0;
        uint32_t frameIndex = 0;
        bool historyValid = false;
        bool exposureValid = false;
        double lastTime = 0.0;
        Math::Matrix4 previousViewProjection;
        bool hasHistory = false;
        std::unordered_map<const void*, Math::Matrix4> previousWorld;
        std::unordered_map<const void*, Math::Matrix4> currentWorld;
    };

    Renderer::Renderer(RHI::Device& device, ShaderLibrary& shaders, GpuResourceCache& resources) : m_device(device), m_shaders(shaders), m_resources(resources)
    {
        m_lightConstants.resize(sizeof(LightConstants));
        m_shadowConstants.resize(sizeof(ShadowConstants));
        std::mt19937 random(1337);
        std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
        for (int i = 0; i < 16; ++i)
        {
            Math::Vector3 sample(distribution(random) * 2.0f - 1.0f, distribution(random) * 2.0f - 1.0f, distribution(random) * 0.9f + 0.1f);
            sample = sample.Normalized() * distribution(random);
            float scale = static_cast<float>(i) / 16.0f;
            scale = Math::Lerp(0.1f, 1.0f, scale * scale);
            sample *= scale;
            m_ssaoKernel[i][0] = sample.x;
            m_ssaoKernel[i][1] = sample.y;
            m_ssaoKernel[i][2] = sample.z;
            m_ssaoKernel[i][3] = 0.0f;
        }
    }

    Renderer::~Renderer() = default;

    void Renderer::ReleaseView(uint64_t viewId)
    {
        m_views.erase(viewId);
    }

    RHI::Texture* Renderer::GetViewDepth(uint64_t viewId) const
    {
        auto it = m_views.find(viewId);
        return it == m_views.end() ? nullptr : it->second->depth.get();
    }

    Renderer::ViewTargets& Renderer::GetTargets(const RenderView& view)
    {
        std::unique_ptr<ViewTargets>& targets = m_views[view.id];
        if (!targets)
        {
            targets = std::make_unique<ViewTargets>();
        }
        if (targets->width != view.width || targets->height != view.height)
        {
            m_device.WaitIdle();
            targets->width = view.width;
            targets->height = view.height;
            auto make = [&](RHI::Format format, uint32_t usage, const char* name)
            {
                RHI::TextureDesc desc;
                desc.width = view.width;
                desc.height = view.height;
                desc.format = format;
                desc.usage = usage;
                desc.debugName = name;
                return m_device.CreateTexture(desc);
            };
            uint32_t renderable = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
            targets->albedo = make(AlbedoFormat, renderable, "GBufferAlbedo");
            targets->normal = make(NormalFormat, renderable, "GBufferNormal");
            targets->material = make(MaterialFormat, renderable, "GBufferMaterial");
            targets->emission = make(EmissionFormat, renderable, "GBufferEmission");
            targets->velocity = make(VelocityFormat, renderable, "Velocity");
            targets->depthCopy = make(DepthCopyFormat, renderable, "SceneDepthCopy");
            targets->depth = make(DepthFormat, RHI::TextureUsageDepthStencil | RHI::TextureUsageSampled, "SceneDepth");
            targets->sceneColor = make(SceneColorFormat, renderable, "SceneColor");
            targets->hasHistory = false;
        }
        return *targets;
    }

    Renderer::MaterialBinding Renderer::ResolveMaterial(const MaterialAsset* material, const EmbeddedMaterial* embedded)
    {
        MaterialBinding binding;
        binding.textures[0] = m_resources.GetWhiteTexture();
        binding.textures[1] = m_resources.GetFlatNormalTexture();
        binding.textures[2] = m_resources.GetWhiteTexture();
        binding.textures[3] = m_resources.GetWhiteTexture();
        binding.textures[4] = m_resources.GetWhiteTexture();
        binding.sampler = m_resources.GetSampler(TextureFilterMode::Anisotropic, TextureWrapMode::Repeat);

        if (!material && embedded)
        {
            Store(binding.baseColor, embedded->baseColor.x, embedded->baseColor.y, embedded->baseColor.z, embedded->baseColor.w);
            binding.surface[0] = embedded->metallic;
            binding.surface[1] = embedded->roughness;
            return binding;
        }
        if (!material)
        {
            return binding;
        }

        Math::Color base = material->m_baseColor;
        Store(binding.baseColor, base.r, base.g, base.b, base.a);
        Math::Color emissive = material->m_emissiveColor * material->m_emissiveIntensity;
        Store(binding.emissive, emissive.r, emissive.g, emissive.b, 1.0f);
        Store(binding.surface, material->m_metallic, material->m_roughness, material->m_normalStrength, material->m_alphaCutoff);
        Store(binding.uvTransform, material->m_tiling.x, material->m_tiling.y, material->m_offset.x, material->m_offset.y);

        auto bind = [&](const Assets::AssetReference<TextureAsset>& reference, int slot, float& flag)
        {
            if (GpuTexture* texture = m_resources.GetTexture(reference.guid))
            {
                binding.textures[slot] = texture->texture.get();
                flag = 1.0f;
                if (slot == 0)
                {
                    binding.sampler = m_resources.GetSampler(texture->filter, texture->wrap);
                }
            }
        };
        bind(material->m_baseColorMap, 0, binding.textureFlags[0]);
        bind(material->m_normalMap, 1, binding.textureFlags[1]);
        bind(material->m_metallicRoughnessMap, 2, binding.textureFlags[2]);
        bind(material->m_occlusionMap, 3, binding.textureFlags[3]);
        bind(material->m_emissiveMap, 4, binding.materialFlags[0]);
        binding.materialFlags[1] = material->m_shadingModel == ShadingModel::Unlit ? 1.0f : 0.0f;
        binding.materialFlags[2] = material->m_blendMode == BlendMode::Masked ? 1.0f : 0.0f;
        binding.transparent = material->m_blendMode == BlendMode::Transparent;
        binding.doubleSided = material->m_doubleSided;
        return binding;
    }

    void Renderer::CollectDraws(const Level::Level* level, const RenderView& view, ViewTargets& targets, const Math::Frustum& frustum)
    {
        m_drawItems.clear();
        targets.currentWorld.clear();
        if (!level)
        {
            return;
        }
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        for (MeshRenderer* renderer : level->FindComponentsOfType<MeshRenderer>())
        {
            const Assets::AssetReference<ModelAsset>& model = renderer->GetModel();
            if (!model.IsSet())
            {
                continue;
            }
            GpuMesh* mesh = m_resources.GetMesh(model.guid, renderer->GetMeshIndex());
            if (!mesh)
            {
                continue;
            }
            Math::Matrix4 world = renderer->GetTransform().GetWorldMatrix();
            Math::Bounds worldBounds = mesh->bounds.Transformed(world);
            targets.currentWorld[renderer] = world;
            auto previous = targets.previousWorld.find(renderer);
            Math::Matrix4 previousWorld = previous == targets.previousWorld.end() ? world : previous->second;
            bool visible = frustum.Intersects(worldBounds);
            if (!visible && !renderer->GetCastShadows())
            {
                ++m_statistics.culledObjects;
                continue;
            }
            if (visible)
            {
                ++m_statistics.visibleObjects;
            }
            else
            {
                ++m_statistics.culledObjects;
            }

            std::shared_ptr<ModelAsset> modelAsset = database.Load<ModelAsset>(model.guid);
            const auto& materials = renderer->GetMaterials();
            for (uint32_t subMesh = 0; subMesh < mesh->subMeshes.size(); ++subMesh)
            {
                uint32_t slot = mesh->subMeshes[subMesh].materialSlot;
                std::shared_ptr<MaterialAsset> material;
                if (slot < materials.size() && materials[slot].IsSet())
                {
                    material = materials[slot].Load();
                }
                else if (!materials.empty() && materials.back().IsSet() && slot >= materials.size())
                {
                    material = materials.back().Load();
                }
                const EmbeddedMaterial* embedded = nullptr;
                if (!material && modelAsset && slot < modelAsset->GetMaterials().size())
                {
                    embedded = &modelAsset->GetMaterials()[slot];
                }
                if (!material && !embedded)
                {
                    material = database.Load<MaterialAsset>(BuiltinAssets::DefaultMaterial());
                }

                DrawItem item;
                item.mesh = mesh;
                item.subMesh = subMesh;
                item.world = world;
                item.previousWorld = previousWorld;
                item.material = ResolveMaterial(material.get(), embedded);
                item.distance = (worldBounds.Center() - view.position).Length();
                item.castShadows = renderer->GetCastShadows() && !item.material.transparent;
                item.objectId = static_cast<uint32_t>(renderer->GetOwner()->GetId().low & 0xFFFFFF);
                if (!visible)
                {
                    item.distance = -1.0f;
                }
                m_drawItems.push_back(item);
            }
        }
        targets.previousWorld = targets.currentWorld;
    }

    void Renderer::CollectLights(const Level::Level* level, const RenderView& view)
    {
        auto* constants = reinterpret_cast<LightConstants*>(m_lightConstants.data());
        std::memset(constants, 0, sizeof(LightConstants));
        Store(constants->ambient, view.environment.ambientDiffuse, view.environment.ambientSpecular, 0.0f, 0.0f);
        m_hasShadowLight = false;
        uint32_t count = 0;
        if (level)
        {
            for (Light* light : level->FindComponentsOfType<Light>())
            {
                if (count >= MaxLights)
                {
                    break;
                }
                GpuLight& gpu = constants->lights[count];
                const Level::Transform& transform = light->GetTransform();
                Math::Vector3 position = transform.GetPosition();
                Math::Vector3 direction = transform.GetForward().Normalized();
                int type = static_cast<int>(light->GetLightType());
                Store(gpu.positionRange, position.x, position.y, position.z, std::max(light->GetRange(), 0.01f));
                Store(gpu.directionType, direction.x, direction.y, direction.z, static_cast<float>(type));
                StoreColor(gpu.colorIntensity, light->GetColor(), light->GetIntensity());
                float inner = std::cos(std::min(light->GetInnerAngle(), light->GetOuterAngle()) * Math::DegreesToRadians);
                float outer = std::cos(light->GetOuterAngle() * Math::DegreesToRadians);
                bool shadow = false;
                if (light->GetLightType() == LightType::Directional && light->GetCastShadows() && !m_hasShadowLight && view.shadows.enabled)
                {
                    shadow = true;
                    m_hasShadowLight = true;
                    m_shadowDirection = direction;
                }
                Store(gpu.spotShadow, inner, outer, shadow ? 1.0f : 0.0f, 0.0f);
                ++count;
            }
        }
        constants->counts[0] = static_cast<float>(count);
        m_lightCount = count;
        m_statistics.lights = count;
    }

    RHI::Pipeline* Renderer::GetMeshPipeline(const char* key, const char* pixelEntry, const char* file, RHI::CullMode cull, const std::vector<RHI::Format>& colors, RHI::Format depth, bool depthWrite, const RHI::BlendState& blend, bool depthClip, int32_t depthBias, float slopeBias)
    {
        std::string fullKey = std::string(key) + "|" + std::to_string(static_cast<int>(cull));
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get(file, "MeshVS", RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get(file, pixelEntry, RHI::ShaderStage::Pixel);
        desc.vertexLayout = MeshLayout();
        desc.rasterizer.cullMode = cull;
        desc.rasterizer.depthClip = depthClip;
        desc.rasterizer.depthBias = depthBias;
        desc.rasterizer.slopeScaledDepthBias = slopeBias;
        desc.depthStencil.depthTest = true;
        desc.depthStencil.depthWrite = depthWrite;
        desc.colorFormats = colors;
        desc.depthFormat = depth;
        for (size_t i = 0; i < colors.size(); ++i)
        {
            desc.blend[i] = blend;
        }
        return m_shaders.GetPipeline(fullKey, desc);
    }

    RHI::Pipeline* Renderer::GetFullscreenPipeline(const char* key, const char* file, const char* entry, RHI::Format format, const RHI::BlendState& blend)
    {
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get(file, "FullscreenVS", RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get(file, entry, RHI::ShaderStage::Pixel);
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.depthStencil.depthTest = false;
        desc.depthStencil.depthWrite = false;
        desc.colorFormats = { format };
        desc.blend[0] = blend;
        return m_shaders.GetPipeline(std::string(key) + "|" + FormatKey(format), desc);
    }

    void Renderer::DrawItems(RHI::CommandList& commands, const std::vector<const DrawItem*>& items, RHI::Pipeline* single, RHI::Pipeline* doubleSided)
    {
        GpuMesh* boundMesh = nullptr;
        for (const DrawItem* item : items)
        {
            RHI::Pipeline* pipeline = item->material.doubleSided ? doubleSided : single;
            if (!pipeline)
            {
                continue;
            }
            commands.SetPipeline(pipeline);
            if (item->mesh != boundMesh)
            {
                commands.SetVertexBuffer(0, item->mesh->vertexBuffer.get());
                commands.SetIndexBuffer(item->mesh->indexBuffer.get(), RHI::IndexFormat::UInt32);
                boundMesh = item->mesh;
            }
            ObjectConstants object;
            object.world = item->world;
            object.previousWorld = item->previousWorld;
            object.normal = item->world.Inverse().Transposed();
            Store(object.params, static_cast<float>(item->objectId & 0xFF) / 255.0f, 0.0f, 0.0f, 0.0f);
            commands.SetConstants(1, object);
            MaterialConstants material;
            std::memcpy(material.baseColor, item->material.baseColor, sizeof(material.baseColor));
            std::memcpy(material.emissive, item->material.emissive, sizeof(material.emissive));
            std::memcpy(material.surface, item->material.surface, sizeof(material.surface));
            std::memcpy(material.uvTransform, item->material.uvTransform, sizeof(material.uvTransform));
            std::memcpy(material.textureFlags, item->material.textureFlags, sizeof(material.textureFlags));
            std::memcpy(material.materialFlags, item->material.materialFlags, sizeof(material.materialFlags));
            commands.SetConstants(2, material);
            for (int slot = 0; slot < 5; ++slot)
            {
                commands.SetTexture(static_cast<uint32_t>(slot), item->material.textures[slot]);
            }
            commands.SetSampler(0, item->material.sampler);
            const SubMesh& subMesh = item->mesh->subMeshes[item->subMesh];
            commands.DrawIndexed(subMesh.indexCount, 1, subMesh.indexOffset, 0, 0);
            ++m_statistics.drawCalls;
        }
    }

    void Renderer::RenderShadows(RHI::CommandList& commands, const RenderView& view, ViewTargets& targets)
    {
        auto* constants = reinterpret_cast<ShadowConstants*>(m_shadowConstants.data());
        std::memset(constants, 0, sizeof(ShadowConstants));
        if (!m_hasShadowLight)
        {
            return;
        }
        uint32_t resolution = std::clamp<uint32_t>(view.shadows.resolution, 256, 8192);
        if (!m_shadowMap || m_shadowResolution != resolution)
        {
            m_device.WaitIdle();
            RHI::TextureDesc desc;
            desc.type = RHI::TextureType::Texture2DArray;
            desc.width = resolution;
            desc.height = resolution;
            desc.layers = CascadeCount;
            desc.format = RHI::Format::Depth32Float;
            desc.usage = RHI::TextureUsageDepthStencil | RHI::TextureUsageSampled;
            desc.debugName = "CascadedShadowMap";
            m_shadowMap = m_device.CreateTexture(desc);
            m_shadowResolution = resolution;
        }

        float nearPlane = view.nearPlane;
        float farPlane = std::min(view.shadows.distance, view.farPlane);
        float splits[CascadeCount];
        for (uint32_t i = 0; i < CascadeCount; ++i)
        {
            float fraction = static_cast<float>(i + 1) / static_cast<float>(CascadeCount);
            float logarithmic = nearPlane * std::pow(farPlane / nearPlane, fraction);
            float uniform = nearPlane + (farPlane - nearPlane) * fraction;
            splits[i] = Math::Lerp(uniform, logarithmic, view.shadows.splitLambda);
        }

        Math::Matrix4 inverseView = view.view.Inverse();
        float aspect = static_cast<float>(view.width) / static_cast<float>(std::max<uint32_t>(view.height, 1));
        float tanHalf = std::tan(view.verticalFov * Math::DegreesToRadians * 0.5f);
        Math::Vector3 up = std::abs(m_shadowDirection.y) > 0.99f ? Math::Vector3::Forward() : Math::Vector3::Up();
        RHI::Pipeline* pipeline = GetMeshPipeline("ShadowDepth", "DepthOnlyPS", "GBuffer.hlsl", RHI::CullMode::None, {}, RHI::Format::Depth32Float, true, {}, false, 0, 2.0f);

        for (uint32_t cascade = 0; cascade < CascadeCount; ++cascade)
        {
            float sliceNear = cascade == 0 ? nearPlane : splits[cascade - 1];
            float sliceFar = splits[cascade];
            Math::Vector3 corners[8];
            Math::Vector3 center;
            for (int i = 0; i < 8; ++i)
            {
                float z = (i & 4) ? sliceFar : sliceNear;
                float halfHeight = z * tanHalf;
                float halfWidth = halfHeight * aspect;
                Math::Vector3 local((i & 1) ? halfWidth : -halfWidth, (i & 2) ? halfHeight : -halfHeight, z);
                corners[i] = inverseView.TransformPoint(local);
                center += corners[i];
            }
            center /= 8.0f;
            float radius = 0.0f;
            for (const Math::Vector3& corner : corners)
            {
                radius = std::max(radius, (corner - center).Length());
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;
            float backDistance = 100.0f;
            Math::Matrix4 lightView = Math::Matrix4::LookAtLH(center - m_shadowDirection * (radius + backDistance), center, up);
            Math::Matrix4 projection = Math::Matrix4::OrthographicOffCenterLH(-radius, radius, -radius, radius, 0.0f, radius * 2.0f + backDistance);
            Math::Matrix4 shadowMatrix = lightView * projection;
            Math::Vector3 origin = shadowMatrix.TransformPoint(Math::Vector3::Zero()) * (static_cast<float>(resolution) * 0.5f);
            Math::Vector3 rounded(std::round(origin.x), std::round(origin.y), 0.0f);
            projection.m[3][0] += (rounded.x - origin.x) * 2.0f / static_cast<float>(resolution);
            projection.m[3][1] += (rounded.y - origin.y) * 2.0f / static_cast<float>(resolution);
            shadowMatrix = lightView * projection;
            constants->cascades[cascade] = shadowMatrix;
            constants->splits[cascade] = sliceFar;
            constants->texelSizes[cascade] = radius * 2.0f / static_cast<float>(resolution);

            RHI::RenderPassDesc pass;
            pass.depth.texture = m_shadowMap.get();
            pass.depth.layer = cascade;
            pass.depth.load = RHI::LoadAction::Clear;
            pass.debugName = "ShadowCascade" + std::to_string(cascade);
            commands.BeginRenderPass(pass);
            ViewConstants viewConstants{};
            viewConstants.viewProjection = shadowMatrix;
            viewConstants.unjitteredViewProjection = shadowMatrix;
            viewConstants.previousViewProjection = shadowMatrix;
            commands.SetConstants(0, viewConstants);
            Math::Frustum frustum = Math::Frustum::FromViewProjection(shadowMatrix);
            std::vector<const DrawItem*> casters;
            for (const DrawItem& item : m_drawItems)
            {
                if (item.castShadows && frustum.Intersects(item.mesh->bounds.Transformed(item.world)))
                {
                    casters.push_back(&item);
                }
            }
            m_statistics.shadowCasters += static_cast<uint32_t>(casters.size());
            DrawItems(commands, casters, pipeline, pipeline);
            commands.EndRenderPass();
        }
        Store(constants->params, 1.0f, 1.0f / static_cast<float>(resolution), view.shadows.normalBias, view.shadows.bias);
    }

    RHI::Pipeline* Renderer::GetSMAAPipeline(const char* vertexEntry, const char* pixelEntry, RHI::Format format)
    {
        RHI::PipelineDesc desc;
        desc.vertexShader = m_shaders.Get("AntiAliasingSMAA.hlsl", vertexEntry, RHI::ShaderStage::Vertex);
        desc.pixelShader = m_shaders.Get("AntiAliasingSMAA.hlsl", pixelEntry, RHI::ShaderStage::Pixel);
        desc.rasterizer.cullMode = RHI::CullMode::None;
        desc.depthStencil.depthTest = false;
        desc.depthStencil.depthWrite = false;
        desc.colorFormats = { format };
        return m_shaders.GetPipeline(std::string("SMAA|") + pixelEntry + "|" + FormatKey(format), desc);
    }

    void Renderer::FullscreenPass(RHI::CommandList& commands, RHI::Texture* target, RHI::Pipeline* pipeline, const char* name, bool load)
    {
        RHI::RenderPassDesc pass;
        pass.colors.push_back({ target, 0, 0, load ? RHI::LoadAction::Load : RHI::LoadAction::DontCare });
        pass.debugName = name;
        commands.BeginRenderPass(pass);
        commands.SetPipeline(pipeline);
    }

    void Renderer::EnsureSMAATextures()
    {
        if (m_smaaArea)
        {
            return;
        }
        RHI::TextureDesc area;
        area.width = AREATEX_WIDTH;
        area.height = AREATEX_HEIGHT;
        area.format = RHI::Format::RG8Unorm;
        area.debugName = "SMAAArea";
        m_smaaArea = m_device.CreateTexture(area);
        m_device.UpdateTexture(m_smaaArea.get(), 0, 0, areaTexBytes, AREATEX_PITCH);
        RHI::TextureDesc search;
        search.width = SEARCHTEX_WIDTH;
        search.height = SEARCHTEX_HEIGHT;
        search.format = RHI::Format::R8Unorm;
        search.debugName = "SMAASearch";
        m_smaaSearch = m_device.CreateTexture(search);
        m_device.UpdateTexture(m_smaaSearch.get(), 0, 0, searchTexBytes, SEARCHTEX_PITCH);
    }

    void Renderer::EnsurePostTargets(const RenderView& view, ViewTargets& targets)
    {
        if (targets.ao && targets.ao->GetWidth() == view.width && targets.ao->GetHeight() == view.height)
        {
            return;
        }
        m_device.WaitIdle();
        uint32_t renderable = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
        auto make = [&](uint32_t width, uint32_t height, RHI::Format format, const char* name)
        {
            RHI::TextureDesc desc;
            desc.width = std::max<uint32_t>(width, 1);
            desc.height = std::max<uint32_t>(height, 1);
            desc.format = format;
            desc.usage = renderable;
            desc.debugName = name;
            return m_device.CreateTexture(desc);
        };
        targets.ao = make(view.width, view.height, RHI::Format::R8Unorm, "SSAO");
        targets.aoBlur = make(view.width, view.height, RHI::Format::R8Unorm, "SSAOBlur");
        targets.bloom.clear();
        uint32_t width = view.width / 2;
        uint32_t height = view.height / 2;
        for (int level = 0; level < 6 && width >= 8 && height >= 8; ++level)
        {
            targets.bloom.push_back(make(width, height, RHI::Format::RGBA16Float, "Bloom"));
            width /= 2;
            height /= 2;
        }
        targets.luminance = make(LuminanceSize, LuminanceSize, RHI::Format::R32Float, "Luminance");
        targets.exposure[0] = make(1, 1, RHI::Format::R32Float, "Exposure0");
        targets.exposure[1] = make(1, 1, RHI::Format::R32Float, "Exposure1");
        targets.history[0] = make(view.width, view.height, SceneColorFormat, "TemporalHistory0");
        targets.history[1] = make(view.width, view.height, SceneColorFormat, "TemporalHistory1");
        targets.ldr = make(view.width, view.height, RHI::Format::RGBA8Unorm, "LDR");
        targets.smaaEdges = make(view.width, view.height, RHI::Format::RG8Unorm, "SMAAEdges");
        targets.smaaBlend = make(view.width, view.height, RHI::Format::RGBA8Unorm, "SMAABlend");
        targets.historyValid = false;
        targets.exposureValid = false;
    }

    RHI::Texture* Renderer::RenderPostProcessing(RHI::CommandList& commands, const RenderView& view, ViewTargets& targets, const void* viewConstants, size_t viewConstantsSize)
    {
        const PostProcessSettings& post = view.post;
        RHI::Texture* hdr = targets.sceneColor.get();
        RHI::Sampler* linear = m_resources.GetLinearClampSampler();
        RHI::Sampler* point = m_resources.GetPointClampSampler();
        auto bindCommon = [&](const PostConstants& constants)
        {
            commands.SetConstants(0, viewConstants, viewConstantsSize);
            commands.SetConstants(1, constants);
            commands.SetSampler(0, linear);
            commands.SetSampler(1, point);
        };

        if (post.antiAliasing == AntiAliasing::TAA && view.debugMode == RenderDebugMode::Lit)
        {
            RHI::Texture* history = targets.history[targets.historyIndex].get();
            RHI::Texture* resolved = targets.history[1 - targets.historyIndex].get();
            FullscreenPass(commands, resolved, GetFullscreenPipeline("TemporalResolve", "PostProcess.hlsl", "TemporalResolvePS", SceneColorFormat), "TemporalResolve");
            PostConstants constants{};
            Store(constants.params, 0.9f, targets.historyValid ? 0.0f : 1.0f, 0.0f, 0.0f);
            Store(constants.sourceSize, static_cast<float>(view.width), static_cast<float>(view.height), 1.0f / view.width, 1.0f / view.height);
            bindCommon(constants);
            commands.SetTexture(0, hdr);
            commands.SetTexture(1, history);
            commands.SetTexture(2, targets.velocity.get());
            commands.Draw(3);
            commands.EndRenderPass();
            targets.historyIndex = 1 - targets.historyIndex;
            targets.historyValid = true;
            hdr = resolved;
        }
        else
        {
            targets.historyValid = false;
        }

        RHI::Texture* bloom = m_resources.GetBlackTexture();
        if (post.bloomIntensity > 0.0f && !targets.bloom.empty() && view.debugMode == RenderDebugMode::Lit)
        {
            PostConstants constants{};
            Store(constants.params, post.bloomThreshold, 1.0f, 0.0f, 0.0f);
            Store(constants.sourceSize, static_cast<float>(hdr->GetWidth()), static_cast<float>(hdr->GetHeight()), 1.0f / hdr->GetWidth(), 1.0f / hdr->GetHeight());
            FullscreenPass(commands, targets.bloom[0].get(), GetFullscreenPipeline("BloomPrefilter", "PostProcess.hlsl", "BloomPrefilterPS", RHI::Format::RGBA16Float), "BloomPrefilter");
            bindCommon(constants);
            commands.SetTexture(0, hdr);
            commands.Draw(3);
            commands.EndRenderPass();
            for (size_t level = 1; level < targets.bloom.size(); ++level)
            {
                RHI::Texture* source = targets.bloom[level - 1].get();
                Store(constants.sourceSize, static_cast<float>(source->GetWidth()), static_cast<float>(source->GetHeight()), 1.0f / source->GetWidth(), 1.0f / source->GetHeight());
                FullscreenPass(commands, targets.bloom[level].get(), GetFullscreenPipeline("BloomDownsample", "PostProcess.hlsl", "BloomDownsamplePS", RHI::Format::RGBA16Float), "BloomDownsample");
                bindCommon(constants);
                commands.SetTexture(0, source);
                commands.Draw(3);
                commands.EndRenderPass();
            }
            for (size_t level = targets.bloom.size() - 1; level > 0; --level)
            {
                RHI::Texture* source = targets.bloom[level].get();
                Store(constants.sourceSize, static_cast<float>(source->GetWidth()), static_cast<float>(source->GetHeight()), 1.0f / source->GetWidth(), 1.0f / source->GetHeight());
                FullscreenPass(commands, targets.bloom[level - 1].get(), GetFullscreenPipeline("BloomUpsample", "PostProcess.hlsl", "BloomUpsamplePS", RHI::Format::RGBA16Float, RHI::BlendState::Additive()), "BloomUpsample", true);
                bindCommon(constants);
                commands.SetTexture(0, source);
                commands.Draw(3);
                commands.EndRenderPass();
            }
            bloom = targets.bloom[0].get();
        }

        RHI::Texture* exposure = m_resources.GetWhiteTexture();
        bool autoExposure = post.autoExposure && view.debugMode == RenderDebugMode::Lit;
        if (autoExposure)
        {
            PostConstants constants{};
            Store(constants.sourceSize, static_cast<float>(hdr->GetWidth()), static_cast<float>(hdr->GetHeight()), 1.0f / hdr->GetWidth(), 1.0f / hdr->GetHeight());
            FullscreenPass(commands, targets.luminance.get(), GetFullscreenPipeline("LogLuminance", "PostProcess.hlsl", "LogLuminancePS", RHI::Format::R32Float), "LogLuminance");
            bindCommon(constants);
            commands.SetTexture(0, hdr);
            commands.Draw(3);
            commands.EndRenderPass();
            double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            float deltaTime = targets.lastTime > 0.0 ? static_cast<float>(std::min(now - targets.lastTime, 0.25)) : 0.0f;
            targets.lastTime = now;
            RHI::Texture* previous = targets.exposure[targets.exposureIndex].get();
            RHI::Texture* current = targets.exposure[1 - targets.exposureIndex].get();
            Store(constants.params, post.exposure, post.autoExposureMin, post.autoExposureMax, post.autoExposureSpeed);
            Store(constants.params2, static_cast<float>(LuminanceSize), targets.exposureValid ? 0.0f : 1.0f, deltaTime, 0.0f);
            FullscreenPass(commands, current, GetFullscreenPipeline("ExposureAdapt", "PostProcess.hlsl", "ExposureAdaptPS", RHI::Format::R32Float), "ExposureAdapt");
            bindCommon(constants);
            commands.SetTexture(0, targets.luminance.get());
            commands.SetTexture(1, previous);
            commands.Draw(3);
            commands.EndRenderPass();
            targets.exposureIndex = 1 - targets.exposureIndex;
            targets.exposureValid = true;
            exposure = current;
        }
        else
        {
            targets.exposureValid = false;
        }

        m_postBloom = bloom;
        m_postExposure = exposure;
        m_postAutoExposure = autoExposure;
        return hdr;
    }

    void Renderer::RenderLines(RHI::CommandList& commands, const RenderView& view, ViewTargets& targets, RHI::Texture* output, const void* viewConstants, size_t viewConstantsSize)
    {
        std::vector<LineVertex> grid;
        if (view.showGrid)
        {
            float spacing = 1.0f;
            int extent = 100;
            Math::Vector3 center(std::round(view.position.x), 0.0f, std::round(view.position.z));
            uint32_t minorColor = view.gridColor.ToRgba8();
            uint32_t majorColor = view.gridMajorColor.ToRgba8();
            uint32_t axisXColor = view.gridAxisXColor.ToRgba8();
            uint32_t axisZColor = view.gridAxisZColor.ToRgba8();
            for (int i = -extent; i <= extent; ++i)
            {
                bool major = (static_cast<int>(center.x) + i) % 10 == 0;
                uint32_t color = major ? majorColor : minorColor;
                float x = center.x + i * spacing;
                grid.push_back({ { x, 0.01f, center.z - extent * spacing }, (static_cast<int>(x) == 0) ? axisZColor : color });
                grid.push_back({ { x, 0.01f, center.z + extent * spacing }, (static_cast<int>(x) == 0) ? axisZColor : color });
                bool majorZ = (static_cast<int>(center.z) + i) % 10 == 0;
                uint32_t colorZ = majorZ ? majorColor : minorColor;
                float z = center.z + i * spacing;
                grid.push_back({ { center.x - extent * spacing, 0.01f, z }, (static_cast<int>(z) == 0) ? axisXColor : colorZ });
                grid.push_back({ { center.x + extent * spacing, 0.01f, z }, (static_cast<int>(z) == 0) ? axisXColor : colorZ });
            }
        }
        const auto& depthTested = DebugDraw::GetDepthTested();
        const auto& overlay = DebugDraw::GetOverlay();
        if (grid.empty() && depthTested.empty() && overlay.empty())
        {
            return;
        }

        auto pipelineFor = [&](bool depthTest)
        {
            RHI::PipelineDesc desc;
            desc.vertexShader = m_shaders.Get("Lines.hlsl", "LineVS", RHI::ShaderStage::Vertex);
            desc.pixelShader = m_shaders.Get("Lines.hlsl", "LinePS", RHI::ShaderStage::Pixel);
            desc.vertexLayout = LineLayout();
            desc.topology = RHI::PrimitiveTopology::LineList;
            desc.rasterizer.cullMode = RHI::CullMode::None;
            desc.depthStencil.depthTest = depthTest;
            desc.depthStencil.depthWrite = false;
            desc.colorFormats = { output->GetFormat() };
            desc.depthFormat = DepthFormat;
            desc.blend[0] = RHI::BlendState::AlphaBlend();
            return m_shaders.GetPipeline(std::string("Lines|") + (depthTest ? "Depth" : "Overlay") + "|" + FormatKey(output->GetFormat()), desc);
        };

        RHI::RenderPassDesc pass;
        pass.colors.push_back({ output, 0, 0, RHI::LoadAction::Load });
        pass.depth.texture = targets.depth.get();
        pass.depth.load = RHI::LoadAction::Load;
        pass.debugName = "Lines";
        commands.BeginRenderPass(pass);
        commands.SetConstants(0, viewConstants, viewConstantsSize);
        LineConstants constants{};
        if (!grid.empty())
        {
            commands.SetPipeline(pipelineFor(true));
            Store(constants.params, 80.0f, 0.0f, 0.0f, 0.0f);
            commands.SetConstants(1, constants);
            commands.SetVertexData(0, grid.data(), grid.size() * sizeof(LineVertex));
            commands.Draw(static_cast<uint32_t>(grid.size()));
        }
        Store(constants.params, 0.0f, 0.0f, 0.0f, 0.0f);
        if (!depthTested.empty())
        {
            commands.SetPipeline(pipelineFor(true));
            commands.SetConstants(1, constants);
            commands.SetVertexData(0, depthTested.data(), depthTested.size() * sizeof(LineVertex));
            commands.Draw(static_cast<uint32_t>(depthTested.size()));
        }
        if (!overlay.empty())
        {
            commands.SetPipeline(pipelineFor(false));
            commands.SetConstants(1, constants);
            commands.SetVertexData(0, overlay.data(), overlay.size() * sizeof(LineVertex));
            commands.Draw(static_cast<uint32_t>(overlay.size()));
        }
        commands.EndRenderPass();
    }

    void Renderer::Render(RHI::CommandList& commands, const Level::Level* level, const RenderView& view, RHI::Texture* output)
    {
        if (!output || view.width == 0 || view.height == 0)
        {
            return;
        }
        m_statistics = {};
        ViewTargets& targets = GetTargets(view);

        EnsurePostTargets(view, targets);
        Math::Matrix4 viewProjection = view.view * view.projection;
        Math::Frustum frustum = Math::Frustum::FromViewProjection(viewProjection);
        Math::Matrix4 jitteredProjection = view.projection;
        if (view.post.antiAliasing == AntiAliasing::TAA && view.debugMode == RenderDebugMode::Lit)
        {
            uint32_t sample = (targets.frameIndex % 8) + 1;
            float jitterX = Halton(sample, 2) - 0.5f;
            float jitterY = Halton(sample, 3) - 0.5f;
            jitteredProjection.m[2][0] += jitterX * 2.0f / static_cast<float>(view.width);
            jitteredProjection.m[2][1] -= jitterY * 2.0f / static_cast<float>(view.height);
        }
        ++targets.frameIndex;
        CollectDraws(level, view, targets, frustum);
        CollectLights(level, view);

        ViewConstants viewConstants{};
        viewConstants.view = view.view;
        viewConstants.projection = view.projection;
        viewConstants.viewProjection = view.view * jitteredProjection;
        viewConstants.inverseViewProjection = viewProjection.Inverse();
        viewConstants.unjitteredViewProjection = viewProjection;
        viewConstants.previousViewProjection = targets.hasHistory ? targets.previousViewProjection : viewProjection;
        Store(viewConstants.cameraPosition, view.position.x, view.position.y, view.position.z, level ? level->GetTime() : 0.0f);
        Store(viewConstants.screenSize, static_cast<float>(view.width), static_cast<float>(view.height), 1.0f / static_cast<float>(view.width), 1.0f / static_cast<float>(view.height));
        Store(viewConstants.clipPlanes, view.nearPlane, view.farPlane, 0.0f, 0.0f);
        StoreColor(viewConstants.skyTop, view.environment.skyTop, view.environment.skyIntensity);
        StoreColor(viewConstants.skyHorizon, view.environment.skyHorizon, 1.0f);
        StoreColor(viewConstants.skyGround, view.environment.skyGround, 1.0f);
        Store(viewConstants.viewFlags, static_cast<float>(view.debugMode), 0.0f, 0.0f, 0.0f);

        RenderShadows(commands, view, targets);

        std::vector<const DrawItem*> opaque;
        std::vector<const DrawItem*> transparent;
        for (const DrawItem& item : m_drawItems)
        {
            if (item.distance < 0.0f)
            {
                continue;
            }
            (item.material.transparent ? transparent : opaque).push_back(&item);
        }
        std::sort(opaque.begin(), opaque.end(), [](const DrawItem* a, const DrawItem* b) { return a->distance < b->distance; });
        std::sort(transparent.begin(), transparent.end(), [](const DrawItem* a, const DrawItem* b) { return a->distance > b->distance; });

        std::vector<RHI::Format> gbufferFormats = { AlbedoFormat, NormalFormat, MaterialFormat, EmissionFormat, VelocityFormat, DepthCopyFormat };
        RHI::RenderPassDesc gbufferPass;
        gbufferPass.colors = {
            { targets.albedo.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 1 } },
            { targets.normal.get(), 0, 0, RHI::LoadAction::Clear, { 0, 1, 0, 1 } },
            { targets.material.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } },
            { targets.emission.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } },
            { targets.velocity.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } },
            { targets.depthCopy.get(), 0, 0, RHI::LoadAction::Clear, { 1, 1, 1, 1 } }
        };
        gbufferPass.depth.texture = targets.depth.get();
        gbufferPass.depth.load = RHI::LoadAction::Clear;
        gbufferPass.debugName = "GBuffer";
        commands.BeginRenderPass(gbufferPass);
        commands.SetConstants(0, viewConstants);
        RHI::Pipeline* gbufferBack = GetMeshPipeline("GBuffer", "GBufferPS", "GBuffer.hlsl", RHI::CullMode::Back, gbufferFormats, DepthFormat, true, {});
        RHI::Pipeline* gbufferNone = GetMeshPipeline("GBuffer", "GBufferPS", "GBuffer.hlsl", RHI::CullMode::None, gbufferFormats, DepthFormat, true, {});
        DrawItems(commands, opaque, gbufferBack, gbufferNone);
        commands.EndRenderPass();

        RHI::Texture* ambientOcclusion = m_resources.GetWhiteTexture();
        if (view.post.ssao && (view.debugMode == RenderDebugMode::Lit || view.debugMode == RenderDebugMode::AmbientOcclusion))
        {
            SSAOConstants ssao{};
            Store(ssao.params, view.post.ssaoRadius, view.post.ssaoIntensity, 0.02f, 16.0f);
            std::memcpy(ssao.kernel, m_ssaoKernel, sizeof(m_ssaoKernel));
            FullscreenPass(commands, targets.ao.get(), GetFullscreenPipeline("SSAO", "SSAO.hlsl", "SSAOPS", RHI::Format::R8Unorm), "SSAO");
            commands.SetConstants(0, viewConstants);
            commands.SetConstants(1, ssao);
            commands.SetTexture(0, targets.depthCopy.get());
            commands.SetTexture(1, targets.normal.get());
            commands.SetSampler(0, m_resources.GetPointClampSampler());
            commands.Draw(3);
            commands.EndRenderPass();
            FullscreenPass(commands, targets.aoBlur.get(), GetFullscreenPipeline("SSAOBlur", "SSAO.hlsl", "SSAOBlurPS", RHI::Format::R8Unorm), "SSAOBlur");
            commands.SetConstants(0, viewConstants);
            commands.SetConstants(1, ssao);
            commands.SetTexture(0, targets.depthCopy.get());
            commands.SetTexture(2, targets.ao.get());
            commands.Draw(3);
            commands.EndRenderPass();
            ambientOcclusion = targets.aoBlur.get();
        }

        RHI::Texture* shadowMap = m_hasShadowLight && m_shadowMap ? m_shadowMap.get() : m_resources.GetDummyShadowMap();
        auto bindLighting = [&]()
        {
            commands.SetConstants(0, viewConstants);
            commands.SetConstants(3, m_lightConstants.data(), sizeof(float) * 8 + sizeof(GpuLight) * std::max<uint32_t>(m_lightCount, 1));
            commands.SetConstants(4, m_shadowConstants.data(), m_shadowConstants.size());
            commands.SetTexture(12, shadowMap);
            commands.SetSampler(12, m_resources.GetShadowSampler());
        };

        RHI::RenderPassDesc lightingPass;
        lightingPass.colors.push_back({ targets.sceneColor.get(), 0, 0, RHI::LoadAction::DontCare });
        lightingPass.debugName = "DeferredLighting";
        commands.BeginRenderPass(lightingPass);
        commands.SetPipeline(GetFullscreenPipeline("DeferredLighting", "DeferredLighting.hlsl", "DeferredLightingPS", SceneColorFormat));
        bindLighting();
        commands.SetTexture(0, targets.albedo.get());
        commands.SetTexture(1, targets.normal.get());
        commands.SetTexture(2, targets.material.get());
        commands.SetTexture(3, targets.emission.get());
        commands.SetTexture(4, targets.depthCopy.get());
        commands.SetTexture(5, ambientOcclusion);
        commands.Draw(3);
        commands.EndRenderPass();

        if (!transparent.empty())
        {
            RHI::RenderPassDesc forwardPass;
            forwardPass.colors.push_back({ targets.sceneColor.get(), 0, 0, RHI::LoadAction::Load });
            forwardPass.depth.texture = targets.depth.get();
            forwardPass.depth.load = RHI::LoadAction::Load;
            forwardPass.debugName = "Transparent";
            commands.BeginRenderPass(forwardPass);
            bindLighting();
            RHI::Pipeline* back = GetMeshPipeline("Forward", "ForwardPS", "Forward.hlsl", RHI::CullMode::Back, { SceneColorFormat }, DepthFormat, false, RHI::BlendState::Premultiplied());
            RHI::Pipeline* none = GetMeshPipeline("Forward", "ForwardPS", "Forward.hlsl", RHI::CullMode::None, { SceneColorFormat }, DepthFormat, false, RHI::BlendState::Premultiplied());
            DrawItems(commands, transparent, back, none);
            commands.EndRenderPass();
        }

        RHI::Texture* hdr = RenderPostProcessing(commands, view, targets, &viewConstants, sizeof(viewConstants));
        AntiAliasing antiAliasing = view.post.antiAliasing;
        bool postAA = antiAliasing == AntiAliasing::FXAA || antiAliasing == AntiAliasing::SMAA;
        RHI::Texture* tonemapTarget = postAA ? targets.ldr.get() : output;

        RHI::RenderPassDesc tonemapPass;
        tonemapPass.colors.push_back({ tonemapTarget, 0, 0, RHI::LoadAction::DontCare });
        tonemapPass.debugName = "Tonemap";
        commands.BeginRenderPass(tonemapPass);
        commands.SetPipeline(GetFullscreenPipeline("Tonemap", "Tonemap.hlsl", "TonemapPS", tonemapTarget->GetFormat()));
        TonemapConstants tonemap{};
        const PostProcessSettings& post = view.post;
        Store(tonemap.exposure, post.exposure, m_postAutoExposure ? 1.0f : 0.0f, 0.0f, 0.0f);
        Store(tonemap.bloom, m_postBloom != m_resources.GetBlackTexture() ? post.bloomIntensity : 0.0f, 0.0f, 0.0f, 0.0f);
        Store(tonemap.grading, post.saturation, post.contrast, 0.0f, post.colorGrading ? 1.0f : 0.0f);
        StoreColor(tonemap.tint, post.tint, 1.0f);
        Store(tonemap.vignette, post.vignetteIntensity, post.vignetteRadius, post.vignetteSoftness, 0.0f);
        StoreColor(tonemap.vignetteColor, post.vignetteColor, 1.0f);
        StoreColor(tonemap.lift, post.lift, 0.0f);
        StoreColor(tonemap.gamma, post.gamma, 1.0f);
        StoreColor(tonemap.gain, post.gain, 1.0f);
        commands.SetConstants(0, viewConstants);
        commands.SetConstants(1, tonemap);
        commands.SetTexture(0, hdr);
        commands.SetTexture(1, m_postBloom);
        commands.SetTexture(2, m_postExposure);
        commands.SetTexture(3, m_resources.GetWhiteTexture());
        commands.SetSampler(0, m_resources.GetLinearClampSampler());
        commands.Draw(3);
        commands.EndRenderPass();

        if (antiAliasing == AntiAliasing::FXAA)
        {
            PostConstants constants{};
            Store(constants.sourceSize, static_cast<float>(view.width), static_cast<float>(view.height), 1.0f / view.width, 1.0f / view.height);
            FullscreenPass(commands, output, GetFullscreenPipeline("FXAA", "PostProcess.hlsl", "FXAAPS", output->GetFormat()), "FXAA");
            commands.SetConstants(0, viewConstants);
            commands.SetConstants(1, constants);
            commands.SetSampler(0, m_resources.GetLinearClampSampler());
            commands.SetSampler(1, m_resources.GetPointClampSampler());
            commands.SetTexture(0, targets.ldr.get());
            commands.Draw(3);
            commands.EndRenderPass();
        }
        else if (antiAliasing == AntiAliasing::SMAA)
        {
            EnsureSMAATextures();
            float metrics[4] = { 1.0f / view.width, 1.0f / view.height, static_cast<float>(view.width), static_cast<float>(view.height) };
            auto bindSMAA = [&]()
            {
                commands.SetConstants(0, metrics, sizeof(metrics));
                commands.SetSampler(0, m_resources.GetLinearClampSampler());
                commands.SetSampler(1, m_resources.GetPointClampSampler());
            };
            RHI::RenderPassDesc edges;
            edges.colors.push_back({ targets.smaaEdges.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } });
            edges.debugName = "SMAAEdges";
            commands.BeginRenderPass(edges);
            commands.SetPipeline(GetSMAAPipeline("SMAAEdgeVS", "SMAAEdgePS", RHI::Format::RG8Unorm));
            bindSMAA();
            commands.SetTexture(0, targets.ldr.get());
            commands.Draw(3);
            commands.EndRenderPass();
            RHI::RenderPassDesc blend;
            blend.colors.push_back({ targets.smaaBlend.get(), 0, 0, RHI::LoadAction::Clear, { 0, 0, 0, 0 } });
            blend.debugName = "SMAABlendWeights";
            commands.BeginRenderPass(blend);
            commands.SetPipeline(GetSMAAPipeline("SMAABlendVS", "SMAABlendPS", RHI::Format::RGBA8Unorm));
            bindSMAA();
            commands.SetTexture(1, targets.smaaEdges.get());
            commands.SetTexture(2, m_smaaArea.get());
            commands.SetTexture(3, m_smaaSearch.get());
            commands.Draw(3);
            commands.EndRenderPass();
            RHI::RenderPassDesc neighborhood;
            neighborhood.colors.push_back({ output, 0, 0, RHI::LoadAction::DontCare });
            neighborhood.debugName = "SMAANeighborhood";
            commands.BeginRenderPass(neighborhood);
            commands.SetPipeline(GetSMAAPipeline("SMAANeighborhoodVS", "SMAANeighborhoodPS", output->GetFormat()));
            bindSMAA();
            commands.SetTexture(0, targets.ldr.get());
            commands.SetTexture(4, targets.smaaBlend.get());
            commands.Draw(3);
            commands.EndRenderPass();
        }

        RenderLines(commands, view, targets, output, &viewConstants, sizeof(viewConstants));

        targets.previousViewProjection = viewProjection;
        targets.hasHistory = true;
    }
}
