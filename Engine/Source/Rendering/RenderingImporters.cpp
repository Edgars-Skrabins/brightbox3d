#include "Rendering/RenderingImporters.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "Math/Color.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/ModelAsset.h"
#include "Rendering/TextureAsset.h"
#include "Reflection/PropertyTraits.h"
#include "Serialization/ObjectSerializer.h"

#include <stb_image.h>
#include <ufbx.h>

#include <cstring>
#include <unordered_map>

namespace BrightBox3D::Rendering
{
    namespace
    {
        float SrgbToLinear(float value)
        {
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        float LinearToSrgb(float value)
        {
            return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        }

        std::vector<TextureMip> BuildMips8(const uint8_t* pixels, uint32_t width, uint32_t height, bool srgb, bool normalMap, bool generate)
        {
            std::vector<TextureMip> mips;
            TextureMip base;
            base.width = width;
            base.height = height;
            base.pixels.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
            mips.push_back(std::move(base));
            if (!generate)
            {
                return mips;
            }
            float lookup[256];
            for (int i = 0; i < 256; ++i)
            {
                lookup[i] = srgb ? SrgbToLinear(i / 255.0f) : i / 255.0f;
            }
            while (mips.back().width > 1 || mips.back().height > 1)
            {
                const TextureMip& previous = mips.back();
                TextureMip next;
                next.width = std::max<uint32_t>(previous.width / 2, 1);
                next.height = std::max<uint32_t>(previous.height / 2, 1);
                next.pixels.resize(static_cast<size_t>(next.width) * next.height * 4);
                for (uint32_t y = 0; y < next.height; ++y)
                {
                    for (uint32_t x = 0; x < next.width; ++x)
                    {
                        float sum[4] = { 0, 0, 0, 0 };
                        int samples = 0;
                        for (uint32_t dy = 0; dy < 2; ++dy)
                        {
                            for (uint32_t dx = 0; dx < 2; ++dx)
                            {
                                uint32_t sx = std::min(x * 2 + dx, previous.width - 1);
                                uint32_t sy = std::min(y * 2 + dy, previous.height - 1);
                                const uint8_t* texel = &previous.pixels[(static_cast<size_t>(sy) * previous.width + sx) * 4];
                                for (int c = 0; c < 3; ++c)
                                {
                                    sum[c] += normalMap ? texel[c] / 255.0f * 2.0f - 1.0f : lookup[texel[c]];
                                }
                                sum[3] += texel[3] / 255.0f;
                                ++samples;
                            }
                        }
                        uint8_t* out = &next.pixels[(static_cast<size_t>(y) * next.width + x) * 4];
                        if (normalMap)
                        {
                            Math::Vector3 normal = Math::Vector3(sum[0], sum[1], sum[2]).Normalized();
                            out[0] = static_cast<uint8_t>(std::clamp((normal.x * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                            out[1] = static_cast<uint8_t>(std::clamp((normal.y * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                            out[2] = static_cast<uint8_t>(std::clamp((normal.z * 0.5f + 0.5f) * 255.0f + 0.5f, 0.0f, 255.0f));
                        }
                        else
                        {
                            for (int c = 0; c < 3; ++c)
                            {
                                float value = sum[c] / static_cast<float>(samples);
                                value = srgb ? LinearToSrgb(value) : value;
                                out[c] = static_cast<uint8_t>(std::clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
                            }
                        }
                        out[3] = static_cast<uint8_t>(std::clamp(sum[3] / static_cast<float>(samples) * 255.0f + 0.5f, 0.0f, 255.0f));
                    }
                }
                mips.push_back(std::move(next));
            }
            return mips;
        }

        std::vector<TextureMip> BuildMipsFloat(const float* pixels, uint32_t width, uint32_t height, bool generate)
        {
            std::vector<TextureMip> mips;
            TextureMip base;
            base.width = width;
            base.height = height;
            base.pixels.resize(static_cast<size_t>(width) * height * 16);
            std::memcpy(base.pixels.data(), pixels, base.pixels.size());
            mips.push_back(std::move(base));
            while (generate && (mips.back().width > 1 || mips.back().height > 1))
            {
                const TextureMip& previous = mips.back();
                const float* source = reinterpret_cast<const float*>(previous.pixels.data());
                TextureMip next;
                next.width = std::max<uint32_t>(previous.width / 2, 1);
                next.height = std::max<uint32_t>(previous.height / 2, 1);
                next.pixels.resize(static_cast<size_t>(next.width) * next.height * 16);
                float* target = reinterpret_cast<float*>(next.pixels.data());
                for (uint32_t y = 0; y < next.height; ++y)
                {
                    for (uint32_t x = 0; x < next.width; ++x)
                    {
                        for (int c = 0; c < 4; ++c)
                        {
                            float sum = 0.0f;
                            for (uint32_t dy = 0; dy < 2; ++dy)
                            {
                                for (uint32_t dx = 0; dx < 2; ++dx)
                                {
                                    uint32_t sx = std::min(x * 2 + dx, previous.width - 1);
                                    uint32_t sy = std::min(y * 2 + dy, previous.height - 1);
                                    sum += source[(static_cast<size_t>(sy) * previous.width + sx) * 4 + c];
                                }
                            }
                            target[(static_cast<size_t>(y) * next.width + x) * 4 + c] = sum * 0.25f;
                        }
                    }
                }
                mips.push_back(std::move(next));
            }
            return mips;
        }

        template <typename E>
        E ReadEnumSetting(const Serialization::JsonValue& settings, const char* name, E fallback)
        {
            std::string text = Serialization::GetString(settings, name);
            if (const Reflection::EnumEntry* entry = Reflection::StaticEnum<E>().FindByName(text))
            {
                return static_cast<E>(entry->value);
            }
            return fallback;
        }

        Math::Vector3 ToVector(const ufbx_vec3& value)
        {
            return { static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z) };
        }
    }

    void TextureImporter::WriteDefaultSettings(Serialization::JsonValue& settings, Serialization::JsonAllocator& allocator) const
    {
        settings.AddMember("srgb", true, allocator);
        settings.AddMember("normalMap", false, allocator);
        settings.AddMember("generateMips", true, allocator);
        settings.AddMember("filter", "Trilinear", allocator);
        settings.AddMember("wrap", "Repeat", allocator);
    }

    bool TextureImporter::Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error)
    {
        bool normalMap = Serialization::GetBool(settings, "normalMap", false);
        bool srgb = Serialization::GetBool(settings, "srgb", true) && !normalMap;
        bool generateMips = Serialization::GetBool(settings, "generateMips", true);
        TextureFilterMode filter = ReadEnumSetting(settings, "filter", TextureFilterMode::Trilinear);
        TextureWrapMode wrap = ReadEnumSetting(settings, "wrap", TextureWrapMode::Repeat);

        std::string path = source.string();
        int width = 0;
        int height = 0;
        int channels = 0;
        TextureAsset texture;
        if (stbi_is_hdr(path.c_str()))
        {
            float* pixels = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
            if (!pixels)
            {
                error = stbi_failure_reason();
                return false;
            }
            texture.SetData(TexturePixelFormat::RGBA32Float, BuildMipsFloat(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), generateMips), filter, wrap, false);
            stbi_image_free(pixels);
        }
        else
        {
            stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
            if (!pixels)
            {
                error = stbi_failure_reason();
                return false;
            }
            texture.SetData(srgb ? TexturePixelFormat::RGBA8Srgb : TexturePixelFormat::RGBA8, BuildMips8(pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height), srgb, normalMap, generateMips), filter, wrap, normalMap);
            stbi_image_free(pixels);
        }
        output = texture.Serialize();
        return true;
    }

    std::shared_ptr<Assets::Asset> TextureImporter::Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size, std::string& error)
    {
        auto texture = std::make_shared<TextureAsset>();
        if (!texture->Deserialize(data, size))
        {
            error = "corrupt cooked texture";
            return nullptr;
        }
        return texture;
    }

    void ModelImporter::WriteDefaultSettings(Serialization::JsonValue& settings, Serialization::JsonAllocator& allocator) const
    {
        settings.AddMember("scale", 1.0, allocator);
        settings.AddMember("generateNormals", false, allocator);
    }

    bool ModelImporter::Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Serialization::JsonValue& settings, std::vector<uint8_t>& output, std::string& error)
    {
        float scale = static_cast<float>(Serialization::GetNumber(settings, "scale", 1.0));
        bool generateNormals = Serialization::GetBool(settings, "generateNormals", false);

        ufbx_load_opts options{};
        options.target_axes = ufbx_axes_left_handed_y_up;
        options.target_unit_meters = 1.0f / std::max(scale, 1.0e-6f);
        options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
        options.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
        options.generate_missing_normals = true;
        ufbx_error ufbxError{};
        ufbx_scene* scene = ufbx_load_file(source.string().c_str(), &options, &ufbxError);
        if (!scene)
        {
            char buffer[512];
            ufbx_format_error(buffer, sizeof(buffer), &ufbxError);
            error = buffer;
            return false;
        }

        ModelAsset model;
        std::unordered_map<const ufbx_mesh*, int32_t> meshIndices;
        std::unordered_map<const ufbx_material*, uint32_t> materialIndices;

        auto materialIndex = [&](const ufbx_material* material) -> uint32_t
        {
            auto found = materialIndices.find(material);
            if (found != materialIndices.end())
            {
                return found->second;
            }
            EmbeddedMaterial embedded;
            if (material)
            {
                embedded.name = std::string(material->name.data, material->name.length);
                const ufbx_material_map& baseColor = material->pbr.base_color;
                if (baseColor.has_value)
                {
                    embedded.baseColor = { static_cast<float>(baseColor.value_vec4.x), static_cast<float>(baseColor.value_vec4.y), static_cast<float>(baseColor.value_vec4.z), 1.0f };
                }
                else if (material->fbx.diffuse_color.has_value)
                {
                    const ufbx_vec4& diffuse = material->fbx.diffuse_color.value_vec4;
                    embedded.baseColor = { static_cast<float>(diffuse.x), static_cast<float>(diffuse.y), static_cast<float>(diffuse.z), 1.0f };
                }
                if (material->pbr.metalness.has_value)
                {
                    embedded.metallic = static_cast<float>(material->pbr.metalness.value_real);
                }
                if (material->pbr.roughness.has_value)
                {
                    embedded.roughness = static_cast<float>(material->pbr.roughness.value_real);
                }
                const ufbx_texture* texture = baseColor.texture ? baseColor.texture : material->fbx.diffuse_color.texture;
                if (texture)
                {
                    embedded.baseColorTexture = std::string(texture->relative_filename.data, texture->relative_filename.length);
                }
                if (const ufbx_texture* normal = material->pbr.normal_map.texture ? material->pbr.normal_map.texture : material->fbx.normal_map.texture)
                {
                    embedded.normalTexture = std::string(normal->relative_filename.data, normal->relative_filename.length);
                }
            }
            else
            {
                embedded.name = "Default";
            }
            uint32_t index = static_cast<uint32_t>(model.GetMaterials().size());
            model.GetMaterials().push_back(embedded);
            materialIndices[material] = index;
            return index;
        };

        auto convertMesh = [&](const ufbx_mesh* mesh) -> int32_t
        {
            auto found = meshIndices.find(mesh);
            if (found != meshIndices.end())
            {
                return found->second;
            }
            MeshData data;
            data.name = std::string(mesh->name.data, mesh->name.length);
            if (data.name.empty())
            {
                data.name = "Mesh";
            }
            std::vector<uint32_t> triangleIndices(mesh->max_face_triangles * 3);
            size_t partCount = std::max<size_t>(mesh->material_parts.count, 1);
            for (size_t part = 0; part < partCount; ++part)
            {
                std::vector<MeshVertex> corners;
                const ufbx_material* material = nullptr;
                const uint32_t* faceIndices = nullptr;
                size_t faceCount = mesh->num_faces;
                if (mesh->material_parts.count > 0)
                {
                    const ufbx_mesh_part& meshPart = mesh->material_parts.data[part];
                    faceIndices = meshPart.face_indices.data;
                    faceCount = meshPart.face_indices.count;
                    if (part < mesh->materials.count)
                    {
                        material = mesh->materials.data[part];
                    }
                }
                for (size_t f = 0; f < faceCount; ++f)
                {
                    ufbx_face face = mesh->faces.data[faceIndices ? faceIndices[f] : f];
                    uint32_t triangles = ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), mesh, face);
                    for (uint32_t t = 0; t < triangles * 3; ++t)
                    {
                        uint32_t index = triangleIndices[t];
                        MeshVertex vertex;
                        vertex.position = ToVector(ufbx_get_vertex_vec3(&mesh->vertex_position, index));
                        if (mesh->vertex_normal.exists)
                        {
                            vertex.normal = ToVector(ufbx_get_vertex_vec3(&mesh->vertex_normal, index)).Normalized();
                        }
                        if (mesh->vertex_uv.exists)
                        {
                            ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
                            vertex.uv = { static_cast<float>(uv.x), 1.0f - static_cast<float>(uv.y) };
                        }
                        corners.push_back(vertex);
                    }
                }
                if (corners.empty())
                {
                    continue;
                }
                std::vector<uint32_t> indices(corners.size());
                ufbx_vertex_stream stream = { corners.data(), corners.size(), sizeof(MeshVertex) };
                size_t vertexCount = ufbx_generate_indices(&stream, 1, indices.data(), indices.size(), nullptr, nullptr);
                uint32_t baseVertex = static_cast<uint32_t>(data.vertices.size());
                SubMesh subMesh;
                subMesh.indexOffset = static_cast<uint32_t>(data.indices.size());
                subMesh.indexCount = static_cast<uint32_t>(indices.size());
                subMesh.materialSlot = materialIndex(material);
                data.vertices.insert(data.vertices.end(), corners.begin(), corners.begin() + static_cast<std::ptrdiff_t>(vertexCount));
                for (uint32_t index : indices)
                {
                    data.indices.push_back(baseVertex + index);
                }
                data.subMeshes.push_back(subMesh);
            }
            if (generateNormals)
            {
                data.ComputeNormals();
            }
            data.ComputeTangents();
            data.ComputeBounds();
            int32_t index = static_cast<int32_t>(model.GetMeshes().size());
            model.GetMeshes().push_back(std::move(data));
            meshIndices[mesh] = index;
            return index;
        };

        std::unordered_map<const ufbx_node*, int32_t> nodeIndices;
        for (size_t i = 0; i < scene->nodes.count; ++i)
        {
            const ufbx_node* node = scene->nodes.data[i];
            if (node->is_root)
            {
                continue;
            }
            ModelNode converted;
            converted.name = std::string(node->name.data, node->name.length);
            const ufbx_transform& transform = node->local_transform;
            converted.position = ToVector(transform.translation);
            converted.rotation = { static_cast<float>(transform.rotation.x), static_cast<float>(transform.rotation.y), static_cast<float>(transform.rotation.z), static_cast<float>(transform.rotation.w) };
            converted.scale = ToVector(transform.scale);
            auto parent = nodeIndices.find(node->parent);
            converted.parent = parent == nodeIndices.end() ? -1 : parent->second;
            if (node->mesh)
            {
                converted.mesh = convertMesh(node->mesh);
            }
            nodeIndices[node] = static_cast<int32_t>(model.GetNodes().size());
            model.GetNodes().push_back(converted);
        }
        ufbx_free_scene(scene);

        if (model.GetMeshes().empty())
        {
            error = "the file contains no meshes";
            return false;
        }
        output = model.Serialize();
        return true;
    }

    std::shared_ptr<Assets::Asset> ModelImporter::Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size, std::string& error)
    {
        auto model = std::make_shared<ModelAsset>();
        if (!model->Deserialize(data, size))
        {
            error = "corrupt cooked model";
            return nullptr;
        }
        return model;
    }

    std::shared_ptr<Assets::Asset> MaterialImporter::CreateAsset(Serialization::JsonDocument&& document, std::string& error)
    {
        auto material = std::make_shared<MaterialAsset>();
        Serialization::DeserializeProperties(*material, document);
        return material;
    }

    void RegisterRenderingAssets()
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        database.RegisterImporter(Assets::AssetType::Texture, std::make_shared<TextureImporter>());
        database.RegisterImporter(Assets::AssetType::Model, std::make_shared<ModelImporter>());
        database.RegisterImporter(Assets::AssetType::Material, std::make_shared<MaterialImporter>());
        BuiltinAssets::Register();
    }

    namespace
    {
        struct Registration
        {
            Registration()
            {
                RegisterRenderingAssets();
            }
        };

        Registration s_registration;
    }
}
