#include "Rendering/DrawCollector.h"

#include "Assets/AssetDatabase.h"
#include "Geometry/ModelAsset.h"
#include "Level/Level.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/FrameLighting.h"
#include "Rendering/GpuResourceCache.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/RenderConstants.h"
#include "Rendering/RenderTargets.h"
#include "Rendering/RenderView.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace BrightBox3D::Rendering
{
    namespace
    {
        constexpr uint64_t ObjectIdMask = 0xFFFFFF;
        constexpr float MinimumLightRange = 0.01f;
        constexpr float CulledDistance = -1.0f;
        constexpr float NoLocalShadow = -1.0f;

        LocalShadowCandidate MakeLocalShadowCandidate(const Light& light, uint32_t lightIndex, const Math::Vector3& position, const Math::Vector3& direction)
        {
            LocalShadowCandidate candidate;
            candidate.lightIndex = lightIndex;
            candidate.type = light.GetLightType();
            candidate.position = position;
            candidate.direction = direction;
            candidate.range = std::max(light.GetRange(), MinimumLightRange);
            candidate.outerAngle = light.GetOuterAngle();
            candidate.intensity = light.GetIntensity();
            candidate.maxTileSize = ShadowResolutionSize(light.GetShadowResolution());
            candidate.bias = light.GetShadowBias();
            candidate.normalBias = light.GetShadowNormalBias();
            return candidate;
        }

        enum MaterialTextureSlot : uint32_t
        {
            BaseColorSlot,
            NormalSlot,
            MetallicRoughnessSlot,
            OcclusionSlot,
            EmissiveSlot
        };

        std::shared_ptr<MaterialAsset> LoadOrReport(const Assets::AssetReference<MaterialAsset>& reference)
        {
            Core::Result<std::shared_ptr<MaterialAsset>> material = reference.Load();
            if (!material)
            {
                Assets::ReportLoadFailure(material.GetError());
                return nullptr;
            }
            return *material;
        }

        std::shared_ptr<MaterialAsset> FindSlotMaterial(const std::vector<Assets::AssetReference<MaterialAsset>>& materials, uint32_t slot)
        {
            if (slot < materials.size() && materials[slot].IsSet())
            {
                return LoadOrReport(materials[slot]);
            }
            if (!materials.empty() && materials.back().IsSet() && slot >= materials.size())
            {
                return LoadOrReport(materials.back());
            }
            return nullptr;
        }
    }

    DrawCollector::DrawCollector(GpuResourceCache& resources, RenderStatistics& statistics) : m_resources(resources), m_statistics(statistics)
    {
    }

    MaterialBinding DrawCollector::ResolveMaterial(const MaterialAsset* material, const Geometry::EmbeddedMaterial* embedded)
    {
        MaterialBinding binding;
        binding.textures[BaseColorSlot] = m_resources.GetWhiteTexture();
        binding.textures[NormalSlot] = m_resources.GetFlatNormalTexture();
        binding.textures[MetallicRoughnessSlot] = m_resources.GetWhiteTexture();
        binding.textures[OcclusionSlot] = m_resources.GetWhiteTexture();
        binding.textures[EmissiveSlot] = m_resources.GetWhiteTexture();
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

        auto bind = [&](const Assets::AssetReference<TextureAsset>& reference, MaterialTextureSlot slot, float& flag)
        {
            if (GpuTexture* texture = m_resources.GetTexture(reference.guid))
            {
                binding.textures[slot] = texture->texture.get();
                flag = 1.0f;
                if (slot == BaseColorSlot)
                {
                    binding.sampler = m_resources.GetSampler(texture->filter, texture->wrap);
                }
            }
        };
        bind(material->m_baseColorMap, BaseColorSlot, binding.textureFlags[0]);
        bind(material->m_normalMap, NormalSlot, binding.textureFlags[1]);
        bind(material->m_metallicRoughnessMap, MetallicRoughnessSlot, binding.textureFlags[2]);
        bind(material->m_occlusionMap, OcclusionSlot, binding.textureFlags[3]);
        bind(material->m_emissiveMap, EmissiveSlot, binding.materialFlags[0]);
        binding.materialFlags[1] = material->m_shadingModel == ShadingModel::Unlit ? 1.0f : 0.0f;
        binding.materialFlags[2] = material->m_blendMode == BlendMode::Masked ? 1.0f : 0.0f;
        binding.transparent = material->m_blendMode == BlendMode::Transparent;
        binding.doubleSided = material->m_doubleSided;
        return binding;
    }

    void DrawCollector::CollectDraws(const Level::Level* level, const RenderView& view, RenderTargets& targets, const Math::Frustum& frustum)
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
            const Assets::AssetReference<Geometry::ModelAsset>& model = renderer->GetModel();
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

            Core::Result<std::shared_ptr<Geometry::ModelAsset>> modelAsset = database.Load<Geometry::ModelAsset>(model.guid);
            if (!modelAsset)
            {
                Assets::ReportLoadFailure(modelAsset.GetError());
            }
            for (uint32_t subMesh = 0; subMesh < mesh->subMeshes.size(); ++subMesh)
            {
                uint32_t slot = mesh->subMeshes[subMesh].materialSlot;
                std::shared_ptr<MaterialAsset> material = FindSlotMaterial(renderer->GetMaterials(), slot);
                const Geometry::EmbeddedMaterial* embedded = nullptr;
                if (!material && modelAsset && slot < modelAsset->GetMaterials().size())
                {
                    embedded = &modelAsset->GetMaterials()[slot];
                }
                if (!material && !embedded)
                {
                    material = LoadOrReport(Assets::AssetReference<MaterialAsset>(BuiltinAssets::DefaultMaterial()));
                }

                DrawItem item;
                item.mesh = mesh;
                item.subMesh = subMesh;
                item.world = world;
                item.previousWorld = previousWorld;
                item.material = ResolveMaterial(material.get(), embedded);
                item.distance = visible ? (worldBounds.Center() - view.position).Length() : CulledDistance;
                item.castShadows = renderer->GetCastShadows() && !item.material.transparent;
                item.objectId = static_cast<uint32_t>(renderer->GetOwner()->GetId().low & ObjectIdMask);
                m_drawItems.push_back(item);
            }
        }
        targets.previousWorld = targets.currentWorld;
    }

    void DrawCollector::CollectLights(const Level::Level* level, const RenderView& view, FrameLighting& lighting)
    {
        auto* constants = reinterpret_cast<LightConstants*>(lighting.lightConstants.data());
        std::memset(constants, 0, sizeof(LightConstants));
        Store(constants->ambient, view.environment.ambientDiffuse, view.environment.ambientSpecular, 0.0f, 0.0f);
        lighting.hasShadowLight = false;
        lighting.localShadowCandidates.clear();
        bool localShadows = view.shadows.enabled && view.shadows.localEnabled;
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
                Store(gpu.positionRange, position.x, position.y, position.z, std::max(light->GetRange(), MinimumLightRange));
                Store(gpu.directionType, direction.x, direction.y, direction.z, static_cast<float>(type));
                StoreColor(gpu.colorIntensity, light->GetColor(), light->GetIntensity());
                float inner = std::cos(std::min(light->GetInnerAngle(), light->GetOuterAngle()) * Math::DegreesToRadians);
                float outer = std::cos(light->GetOuterAngle() * Math::DegreesToRadians);
                bool shadow = false;
                if (light->GetLightType() == LightType::Directional && light->GetCastShadows() && !lighting.hasShadowLight && view.shadows.enabled)
                {
                    shadow = true;
                    lighting.hasShadowLight = true;
                    lighting.shadowDirection = direction;
                }
                Store(gpu.spotShadow, inner, outer, shadow ? 1.0f : 0.0f, NoLocalShadow);
                if (light->GetLightType() != LightType::Directional && light->GetCastShadows() && localShadows)
                {
                    lighting.localShadowCandidates.push_back(MakeLocalShadowCandidate(*light, count, position, direction));
                }
                ++count;
            }
        }
        constants->counts[0] = static_cast<float>(count);
        lighting.lightCount = count;
        m_statistics.lights = count;
    }
}
