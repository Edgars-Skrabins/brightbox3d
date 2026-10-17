#include "Thumbnails.h"

#include "Assets/AssetDatabase.h"
#include "Geometry/ModelAsset.h"
#include "Level/Level.h"
#include "Level/LevelSerializer.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureAsset.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr uint64_t ThumbnailViewId = 0x5448554D;
        constexpr const char* ThumbnailLevelName = "Thumbnail";
        constexpr size_t MaxPreviewMeshes = 64;
        constexpr float SphereRadius = 0.5f;
        constexpr Math::Vector3 SunAngles{ 40.0f, -40.0f, 0.0f };
        constexpr float SunIntensity = 3.0f;
        constexpr float MinimumRadius = 0.01f;
        constexpr float VerticalFov = 30.0f;
        constexpr float OrbitPitch = 25.0f;
        constexpr float OrbitYaw = -35.0f;
        constexpr float FramingMargin = 1.05f;
        constexpr float ClipDepthScale = 2.0f;
        constexpr float MinimumNearPlane = 0.01f;
        constexpr Math::Color SkyTop{ 0.2f, 0.2f, 0.22f, 1.0f };
        constexpr Math::Color SkyHorizon{ 0.25f, 0.25f, 0.27f, 1.0f };
        constexpr Math::Color SkyGround{ 0.18f, 0.18f, 0.19f, 1.0f };

        Rendering::MeshRenderer* AddRenderer(Level::Level& preview, const Core::Guid& model, int mesh)
        {
            Level::LevelObject* object = preview.CreateObject(Level::LevelObject::StaticType());
            auto* renderer = object->AddComponent<Rendering::MeshRenderer>();
            renderer->SetModel(Assets::AssetReference<Geometry::ModelAsset>(model));
            renderer->SetMeshIndex(mesh);
            return renderer;
        }

        std::optional<Math::Bounds> AddModel(Level::Level& preview, const Core::Guid& asset)
        {
            auto model = Assets::AssetDatabase::Get().Load<Geometry::ModelAsset>(asset);
            if (!model)
            {
                Assets::ReportLoadFailure(model.GetError());
                return std::nullopt;
            }
            Math::Bounds bounds;
            for (size_t i = 0; i < model->GetMeshes().size() && i < MaxPreviewMeshes; ++i)
            {
                AddRenderer(preview, asset, static_cast<int>(i));
                bounds.Encapsulate(model->GetMeshes()[i].bounds);
            }
            return bounds;
        }

        Math::Bounds AddMaterialSphere(Level::Level& preview, const Core::Guid& asset)
        {
            Rendering::MeshRenderer* renderer = AddRenderer(preview, Rendering::BuiltinAssets::Sphere(), 0);
            renderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(asset));
            return Math::Bounds{ { -SphereRadius, -SphereRadius, -SphereRadius }, { SphereRadius, SphereRadius, SphereRadius } };
        }

        std::optional<Math::Bounds> AddVariant(Level::Level& preview, const Core::Guid& asset)
        {
            if (!Level::LevelSerializer::Instantiate(preview, asset, nullptr, nullptr))
            {
                return std::nullopt;
            }
            Math::Bounds bounds;
            for (Rendering::MeshRenderer* renderer : preview.FindComponentsOfType<Rendering::MeshRenderer>())
            {
                auto model = renderer->GetModel().Load();
                if (const Geometry::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr)
                {
                    bounds.Encapsulate(mesh->bounds.Transformed(renderer->GetTransform().GetWorldMatrix()));
                }
            }
            return bounds.IsValid() ? std::optional<Math::Bounds>(bounds) : std::nullopt;
        }

        std::optional<Math::Bounds> PopulatePreview(Level::Level& preview, const Assets::AssetRecord& record)
        {
            switch (record.type)
            {
            case Assets::AssetType::Model:
                return AddModel(preview, record.guid);
            case Assets::AssetType::Material:
                return AddMaterialSphere(preview, record.guid);
            case Assets::AssetType::LevelObjectVariant:
                return AddVariant(preview, record.guid);
            default:
                return std::nullopt;
            }
        }

        void AddSun(Level::Level& preview)
        {
            Level::LevelObject* sun = preview.CreateObject(Level::LevelObject::StaticType());
            sun->GetTransform().SetLocalEulerAngles(SunAngles);
            auto* light = sun->AddComponent<Rendering::Light>();
            light->SetLightType(Rendering::LightType::Directional);
            light->SetIntensity(SunIntensity);
            light->SetCastShadows(false);
        }

        Rendering::RenderView FramedView(const Math::Bounds& bounds, uint32_t size)
        {
            Math::Vector3 center = bounds.Center();
            float radius = std::max(bounds.Extents().Length(), MinimumRadius);
            Rendering::RenderView view;
            view.id = ThumbnailViewId;
            view.width = size;
            view.height = size;
            view.verticalFov = VerticalFov;
            Math::Quaternion orbit = Math::Quaternion::Euler(OrbitPitch, OrbitYaw, 0.0f);
            float distance = radius / std::sin(view.verticalFov * 0.5f * Math::DegreesToRadians) * FramingMargin;
            view.position = center - orbit * Math::Vector3::Forward() * distance;
            view.nearPlane = std::max(distance - radius * ClipDepthScale, MinimumNearPlane);
            view.farPlane = distance + radius * ClipDepthScale;
            view.view = Math::Matrix4::LookAtLH(view.position, center, Math::Vector3::Up());
            view.projection = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, 1.0f, view.nearPlane, view.farPlane);
            view.shadows.enabled = false;
            view.post.antiAliasing = Rendering::AntiAliasing::FXAA;
            view.post.ssao = false;
            view.environment.skyTop = SkyTop;
            view.environment.skyHorizon = SkyHorizon;
            view.environment.skyGround = SkyGround;
            return view;
        }
    }

    ThumbnailCache::ThumbnailCache(RHI::Device& device, Rendering::Renderer& renderer) : m_device(device), m_renderer(renderer)
    {
        m_changedHandle = Assets::AssetDatabase::Get().OnAssetChanged.Subscribe([this](const Core::Guid& guid)
        {
            auto it = m_thumbnails.find(guid);
            if (it != m_thumbnails.end())
            {
                m_retired.push_back(std::move(it->second));
                m_thumbnails.erase(it);
            }
            m_failed.erase(guid);
        });
    }

    ThumbnailCache::~ThumbnailCache()
    {
        Assets::AssetDatabase::Get().OnAssetChanged.Unsubscribe(m_changedHandle);
        m_device.WaitIdle();
        m_renderer.ReleaseView(ThumbnailViewId);
    }

    void ThumbnailCache::Clear()
    {
        m_device.WaitIdle();
        m_thumbnails.clear();
        m_failed.clear();
        m_queue.clear();
        m_queued.clear();
        m_retired.clear();
    }

    RHI::Texture* ThumbnailCache::Get(const Core::Guid& asset)
    {
        auto it = m_thumbnails.find(asset);
        if (it != m_thumbnails.end())
        {
            return it->second.get();
        }
        if (!m_failed.count(asset) && !m_queued.count(asset))
        {
            m_queue.push_back(asset);
            m_queued.insert(asset);
        }
        return nullptr;
    }

    void ThumbnailCache::Update(RHI::CommandList& commands)
    {
        if (m_retired.size() > 32)
        {
            m_device.WaitIdle();
            m_retired.clear();
        }
        int budget = 2;
        while (budget > 0 && !m_queue.empty())
        {
            Core::Guid asset = m_queue.front();
            m_queue.pop_front();
            m_queued.erase(asset);
            if (!Generate(commands, asset))
            {
                m_failed.insert(asset);
            }
            --budget;
        }
    }

    bool ThumbnailCache::CopyTextureThumbnail(const Core::Guid& asset)
    {
        auto texture = Assets::AssetDatabase::Get().Load<Rendering::TextureAsset>(asset);
        if (!texture)
        {
            Assets::ReportLoadFailure(texture.GetError());
            return false;
        }
        if (texture->GetMips().empty() || texture->GetPixelFormat() == Rendering::TexturePixelFormat::RGBA32Float)
        {
            return false;
        }
        const Rendering::TextureMip* chosen = &texture->GetMips().front();
        for (const Rendering::TextureMip& mip : texture->GetMips())
        {
            if (mip.width <= Size && mip.height <= Size)
            {
                chosen = &mip;
                break;
            }
        }
        RHI::TextureDesc imageDesc;
        imageDesc.width = chosen->width;
        imageDesc.height = chosen->height;
        imageDesc.format = RHI::Format::RGBA8Unorm;
        imageDesc.debugName = "TextureThumbnail";
        auto image = m_device.CreateTexture(imageDesc);
        m_device.UpdateTexture(image.get(), 0, 0, chosen->pixels.data(), chosen->width * 4);
        m_thumbnails[asset] = std::move(image);
        return true;
    }

    bool ThumbnailCache::RenderPreview(RHI::CommandList& commands, const Assets::AssetRecord& record)
    {
        Level::Level preview(ThumbnailLevelName);
        std::optional<Math::Bounds> bounds = PopulatePreview(preview, record);
        if (!bounds)
        {
            return false;
        }
        AddSun(preview);
        RHI::TextureDesc desc;
        desc.width = Size;
        desc.height = Size;
        desc.format = RHI::Format::RGBA8Unorm;
        desc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
        desc.debugName = "Thumbnail";
        auto target = m_device.CreateTexture(desc);
        m_renderer.Render(commands, &preview, FramedView(*bounds, Size), target.get());
        m_thumbnails[record.guid] = std::move(target);
        return true;
    }

    bool ThumbnailCache::Generate(RHI::CommandList& commands, const Core::Guid& asset)
    {
        const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecord(asset);
        if (!record)
        {
            return false;
        }
        if (record->type == Assets::AssetType::Texture)
        {
            return CopyTextureThumbnail(asset);
        }
        return RenderPreview(commands, *record);
    }
}
