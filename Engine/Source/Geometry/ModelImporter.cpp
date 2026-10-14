#include "Geometry/ModelImporter.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Geometry/ModelAsset.h"
#include "Geometry/ModelImportSettings.h"

#include <ufbx.h>

#include <unordered_map>

namespace BrightBox3D::Geometry
{
    namespace
    {
        constexpr float MinimumImportScale = 1.0e-6f;
        constexpr size_t ErrorBufferSize = 512;
        constexpr uint32_t TriangleCorners = 3;
        constexpr const char* DefaultMaterialName = "Default";
        constexpr const char* DefaultMeshName = "Mesh";

        Math::Vector3 ToVector(const ufbx_vec3& value)
        {
            return { static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z) };
        }

        std::string ToString(const ufbx_string& value)
        {
            return std::string(value.data, value.length);
        }

        ufbx_scene* LoadScene(const std::filesystem::path& source, float scale, std::string& error)
        {
            ufbx_load_opts options{};
            options.target_axes = ufbx_axes_left_handed_y_up;
            options.target_unit_meters = 1.0f / std::max(scale, MinimumImportScale);
            options.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
            options.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
            options.generate_missing_normals = true;
            ufbx_error ufbxError{};
            ufbx_scene* scene = ufbx_load_file(source.string().c_str(), &options, &ufbxError);
            if (!scene)
            {
                char buffer[ErrorBufferSize];
                ufbx_format_error(buffer, sizeof(buffer), &ufbxError);
                error = buffer;
            }
            return scene;
        }

        EmbeddedMaterial ConvertMaterial(const ufbx_material* material)
        {
            EmbeddedMaterial embedded;
            if (!material)
            {
                embedded.name = DefaultMaterialName;
                return embedded;
            }
            embedded.name = ToString(material->name);
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
            if (const ufbx_texture* texture = baseColor.texture ? baseColor.texture : material->fbx.diffuse_color.texture)
            {
                embedded.baseColorTexture = ToString(texture->relative_filename);
            }
            if (const ufbx_texture* normal = material->pbr.normal_map.texture ? material->pbr.normal_map.texture : material->fbx.normal_map.texture)
            {
                embedded.normalTexture = ToString(normal->relative_filename);
            }
            return embedded;
        }

        MeshVertex ReadVertex(const ufbx_mesh& mesh, uint32_t index)
        {
            MeshVertex vertex;
            vertex.position = ToVector(ufbx_get_vertex_vec3(&mesh.vertex_position, index));
            if (mesh.vertex_normal.exists)
            {
                vertex.normal = ToVector(ufbx_get_vertex_vec3(&mesh.vertex_normal, index)).Normalized();
            }
            if (mesh.vertex_uv.exists)
            {
                ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh.vertex_uv, index);
                vertex.uv = { static_cast<float>(uv.x), 1.0f - static_cast<float>(uv.y) };
            }
            return vertex;
        }

        class SceneConverter
        {
        public:
            explicit SceneConverter(bool generateNormals) : m_generateNormals(generateNormals) {}

            ModelAsset& GetModel() { return m_model; }

            void ExtractMeshes(const ufbx_scene& scene)
            {
                std::unordered_map<const ufbx_node*, int32_t> nodeIndices;
                for (size_t i = 0; i < scene.nodes.count; ++i)
                {
                    const ufbx_node* node = scene.nodes.data[i];
                    if (node->is_root)
                    {
                        continue;
                    }
                    ModelNode converted;
                    converted.name = ToString(node->name);
                    const ufbx_transform& transform = node->local_transform;
                    converted.position = ToVector(transform.translation);
                    converted.rotation = { static_cast<float>(transform.rotation.x), static_cast<float>(transform.rotation.y), static_cast<float>(transform.rotation.z), static_cast<float>(transform.rotation.w) };
                    converted.scale = ToVector(transform.scale);
                    auto parent = nodeIndices.find(node->parent);
                    converted.parent = parent == nodeIndices.end() ? -1 : parent->second;
                    if (node->mesh)
                    {
                        converted.mesh = ExtractMesh(*node->mesh);
                    }
                    nodeIndices[node] = static_cast<int32_t>(m_model.GetNodes().size());
                    m_model.GetNodes().push_back(converted);
                }
            }

        private:
            uint32_t MapMaterial(const ufbx_material* material)
            {
                auto found = m_materialIndices.find(material);
                if (found != m_materialIndices.end())
                {
                    return found->second;
                }
                uint32_t index = static_cast<uint32_t>(m_model.GetMaterials().size());
                m_model.GetMaterials().push_back(ConvertMaterial(material));
                m_materialIndices[material] = index;
                return index;
            }

            std::vector<MeshVertex> TriangulatePart(const ufbx_mesh& mesh, size_t part, std::vector<uint32_t>& triangleIndices, const ufbx_material*& material) const
            {
                std::vector<MeshVertex> corners;
                const uint32_t* faceIndices = nullptr;
                size_t faceCount = mesh.num_faces;
                if (mesh.material_parts.count > 0)
                {
                    const ufbx_mesh_part& meshPart = mesh.material_parts.data[part];
                    faceIndices = meshPart.face_indices.data;
                    faceCount = meshPart.face_indices.count;
                    if (part < mesh.materials.count)
                    {
                        material = mesh.materials.data[part];
                    }
                }
                for (size_t f = 0; f < faceCount; ++f)
                {
                    ufbx_face face = mesh.faces.data[faceIndices ? faceIndices[f] : f];
                    uint32_t triangles = ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), &mesh, face);
                    for (uint32_t t = 0; t < triangles * TriangleCorners; ++t)
                    {
                        corners.push_back(ReadVertex(mesh, triangleIndices[t]));
                    }
                }
                return corners;
            }

            void AppendPart(MeshData& data, std::vector<MeshVertex>& corners, const ufbx_material* material)
            {
                std::vector<uint32_t> indices(corners.size());
                ufbx_vertex_stream stream = { corners.data(), corners.size(), sizeof(MeshVertex) };
                size_t vertexCount = ufbx_generate_indices(&stream, 1, indices.data(), indices.size(), nullptr, nullptr);
                uint32_t baseVertex = static_cast<uint32_t>(data.vertices.size());
                SubMesh subMesh;
                subMesh.indexOffset = static_cast<uint32_t>(data.indices.size());
                subMesh.indexCount = static_cast<uint32_t>(indices.size());
                subMesh.materialSlot = MapMaterial(material);
                data.vertices.insert(data.vertices.end(), corners.begin(), corners.begin() + static_cast<std::ptrdiff_t>(vertexCount));
                for (uint32_t index : indices)
                {
                    data.indices.push_back(baseVertex + index);
                }
                data.subMeshes.push_back(subMesh);
            }

            int32_t ExtractMesh(const ufbx_mesh& mesh)
            {
                auto found = m_meshIndices.find(&mesh);
                if (found != m_meshIndices.end())
                {
                    return found->second;
                }
                MeshData data;
                data.name = ToString(mesh.name);
                if (data.name.empty())
                {
                    data.name = DefaultMeshName;
                }
                std::vector<uint32_t> triangleIndices(mesh.max_face_triangles * TriangleCorners);
                size_t partCount = std::max<size_t>(mesh.material_parts.count, 1);
                for (size_t part = 0; part < partCount; ++part)
                {
                    const ufbx_material* material = nullptr;
                    std::vector<MeshVertex> corners = TriangulatePart(mesh, part, triangleIndices, material);
                    if (!corners.empty())
                    {
                        AppendPart(data, corners, material);
                    }
                }
                if (m_generateNormals)
                {
                    data.ComputeNormals();
                }
                data.ComputeTangents();
                data.ComputeBounds();
                int32_t index = static_cast<int32_t>(m_model.GetMeshes().size());
                m_model.GetMeshes().push_back(std::move(data));
                m_meshIndices[&mesh] = index;
                return index;
            }

            bool m_generateNormals = false;
            ModelAsset m_model;
            std::unordered_map<const ufbx_mesh*, int32_t> m_meshIndices;
            std::unordered_map<const ufbx_material*, uint32_t> m_materialIndices;
        };

        bool WriteModel(const ModelAsset& model, std::vector<uint8_t>& output, std::string& error)
        {
            if (model.GetMeshes().empty())
            {
                error = Core::Text(Core::EngineText::ModelHasNoMeshes);
                return false;
            }
            output = model.Serialize();
            return true;
        }
    }

    const Reflection::TypeInfo* ModelImporter::GetSettingsType() const
    {
        return &ModelImportSettings::StaticType();
    }

    bool ModelImporter::Cook(const Assets::AssetRecord& record, const std::filesystem::path& source, const Reflection::EngineObject* settings, std::vector<uint8_t>& output, std::string& error)
    {
        const auto& importSettings = static_cast<const ModelImportSettings&>(*settings);
        ufbx_scene* scene = LoadScene(source, importSettings.scale, error);
        if (!scene)
        {
            return false;
        }
        SceneConverter converter(importSettings.generateNormals);
        converter.ExtractMeshes(*scene);
        ufbx_free_scene(scene);
        return WriteModel(converter.GetModel(), output, error);
    }

    std::shared_ptr<Assets::Asset> ModelImporter::Load(const Assets::AssetRecord& record, const uint8_t* data, size_t size, std::string& error)
    {
        auto model = std::make_shared<ModelAsset>();
        if (!model->Deserialize(data, size))
        {
            error = Core::Text(Core::EngineText::CorruptCookedModel);
            return nullptr;
        }
        return model;
    }

    void RegisterGeometryAssets()
    {
        Assets::AssetDatabase::Get().RegisterImporter(Assets::AssetType::Model, std::make_shared<ModelImporter>());
    }

    namespace
    {
        struct Registration
        {
            Registration()
            {
                RegisterGeometryAssets();
            }
        };

        Registration s_registration;
    }
}
