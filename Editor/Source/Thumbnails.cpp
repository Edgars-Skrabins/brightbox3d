#include "Thumbnails.h"

#include "Assets/AssetDatabase.h"
#include "Level/Level.h"
#include "Level/LevelSerializer.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/Light.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/ModelAsset.h"
#include "Rendering/Renderer.h"
#include "Rendering/TextureAsset.h"

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr uint64_t ThumbnailViewId = 0x5448554D;
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

    bool ThumbnailCache::Generate(RHI::CommandList& commands, const Core::Guid& asset)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        const Assets::AssetRecord* record = database.FindRecord(asset);
        if (!record)
        {
            return false;
        }
        RHI::TextureDesc desc;
        desc.width = Size;
        desc.height = Size;
        desc.format = RHI::Format::RGBA8Unorm;
        desc.usage = RHI::TextureUsageRenderTarget | RHI::TextureUsageSampled;
        desc.debugName = "Thumbnail";

        if (record->type == Assets::AssetType::Texture)
        {
            auto texture = database.Load<Rendering::TextureAsset>(asset);
            if (!texture || texture->GetMips().empty() || texture->GetPixelFormat() == Rendering::TexturePixelFormat::RGBA32Float)
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

        Level::Level preview("Thumbnail");
        Math::Bounds bounds;
        auto addRenderer = [&](Level::LevelObjectInstance& object, const Core::Guid& model, int mesh)
        {
            auto* renderer = object.AddComponent<Rendering::MeshRenderer>();
            renderer->SetModel(Assets::AssetReference<Rendering::ModelAsset>(model));
            renderer->SetMeshIndex(mesh);
            return renderer;
        };
        if (record->type == Assets::AssetType::Model)
        {
            auto model = database.Load<Rendering::ModelAsset>(asset);
            if (!model)
            {
                return false;
            }
            for (size_t i = 0; i < model->GetMeshes().size() && i < 64; ++i)
            {
                Level::LevelObjectInstance* object = preview.CreateObject(Level::LevelObjectInstance::StaticType());
                addRenderer(*object, asset, static_cast<int>(i));
                bounds.Encapsulate(model->GetMeshes()[i].bounds);
            }
        }
        else if (record->type == Assets::AssetType::Material)
        {
            Level::LevelObjectInstance* object = preview.CreateObject(Level::LevelObjectInstance::StaticType());
            auto* renderer = addRenderer(*object, Rendering::BuiltinAssets::Sphere(), 0);
            renderer->SetMaterial(0, Assets::AssetReference<Rendering::MaterialAsset>(asset));
            bounds.Encapsulate(Math::Bounds{ { -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f } });
        }
        else if (record->type == Assets::AssetType::LevelObject)
        {
            Level::LevelObjectInstance* root = Level::LevelSerializer::Instantiate(preview, asset, nullptr, nullptr);
            if (!root)
            {
                return false;
            }
            for (Rendering::MeshRenderer* renderer : preview.FindComponentsOfType<Rendering::MeshRenderer>())
            {
                auto model = renderer->GetModel().Load();
                if (const Rendering::MeshData* mesh = model ? model->GetMesh(renderer->GetMeshIndex()) : nullptr)
                {
                    bounds.Encapsulate(mesh->bounds.Transformed(renderer->GetTransform().GetWorldMatrix()));
                }
            }
            if (!bounds.IsValid())
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        Level::LevelObjectInstance* sun = preview.CreateObject(Level::LevelObjectInstance::StaticType());
        sun->GetTransform().SetLocalEulerAngles({ 40.0f, -40.0f, 0.0f });
        auto* light = sun->AddComponent<Rendering::Light>();
        light->SetLightType(Rendering::LightType::Directional);
        light->SetIntensity(3.0f);
        light->SetCastShadows(false);

        Math::Vector3 center = bounds.Center();
        float radius = std::max(bounds.Extents().Length(), 0.01f);
        Rendering::RenderView view;
        view.id = ThumbnailViewId;
        view.width = Size;
        view.height = Size;
        view.verticalFov = 30.0f;
        Math::Quaternion orbit = Math::Quaternion::Euler(25.0f, -35.0f, 0.0f);
        float distance = radius / std::sin(view.verticalFov * 0.5f * Math::DegreesToRadians) * 1.05f;
        view.position = center - orbit * Math::Vector3::Forward() * distance;
        view.nearPlane = std::max(distance - radius * 2.0f, 0.01f);
        view.farPlane = distance + radius * 2.0f;
        view.view = Math::Matrix4::LookAtLH(view.position, center, Math::Vector3::Up());
        view.projection = Math::Matrix4::PerspectiveFovLH(view.verticalFov * Math::DegreesToRadians, 1.0f, view.nearPlane, view.farPlane);
        view.shadows.enabled = false;
        view.post.antiAliasing = Rendering::AntiAliasing::FXAA;
        view.post.ssao = false;
        view.environment.skyTop = { 0.2f, 0.2f, 0.22f, 1.0f };
        view.environment.skyHorizon = { 0.25f, 0.25f, 0.27f, 1.0f };
        view.environment.skyGround = { 0.18f, 0.18f, 0.19f, 1.0f };

        auto target = m_device.CreateTexture(desc);
        m_renderer.Render(commands, &preview, view, target.get());
        m_thumbnails[asset] = std::move(target);
        return true;
    }
}
