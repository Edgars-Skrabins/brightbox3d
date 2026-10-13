#include "Assets/AssetDatabase.h"
#include "Geometry/ModelAsset.h"
#include "Rendering/BuiltinAssets.h"
#include "Rendering/MaterialAsset.h"
#include "Rendering/TextureAsset.h"
#include "Serialization/ObjectSerializer.h"

#include <gtest/gtest.h>

#include <stb_image_write.h>

#include <filesystem>

using namespace BrightBox3D;
using namespace BrightBox3D::Geometry;
using namespace BrightBox3D::Rendering;

namespace
{
    class RenderingProject
    {
    public:
        RenderingProject()
        {
            m_root = std::filesystem::temp_directory_path() / ("BrightBox3DRendering_" + Core::Guid::Generate().ToString());
            std::filesystem::create_directories(m_root / "Assets");
        }

        ~RenderingProject()
        {
            Assets::AssetDatabase::Get().Close();
            std::error_code error;
            std::filesystem::remove_all(m_root, error);
        }

        std::filesystem::path Assets() const { return m_root / "Assets"; }
        void Open() { Assets::AssetDatabase::Get().Open(m_root); }

    private:
        std::filesystem::path m_root;
    };

    void ExpectOutwardWinding(const MeshData& mesh)
    {
        int consistent = 0;
        int total = 0;
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            const MeshVertex& a = mesh.vertices[mesh.indices[i]];
            const MeshVertex& b = mesh.vertices[mesh.indices[i + 1]];
            const MeshVertex& c = mesh.vertices[mesh.indices[i + 2]];
            Math::Vector3 face = Math::Vector3::Cross(b.position - a.position, c.position - a.position);
            if (face.LengthSquared() < 1.0e-12f)
            {
                continue;
            }
            Math::Vector3 normal = a.normal + b.normal + c.normal;
            consistent += Math::Vector3::Dot(face, normal) > 0.0f ? 1 : 0;
            ++total;
        }
        ASSERT_GT(total, 0);
        EXPECT_EQ(consistent, total) << mesh.name;
    }
}

TEST(RenderingAssetTests, BuiltinMeshesAreClosedAndWoundConsistently)
{
    for (const MeshData& mesh : { BuiltinAssets::CreateCube(), BuiltinAssets::CreateSphere(), BuiltinAssets::CreatePlane(), BuiltinAssets::CreateQuad(), BuiltinAssets::CreateCylinder(), BuiltinAssets::CreateCapsule() })
    {
        ExpectOutwardWinding(mesh);
        EXPECT_TRUE(mesh.bounds.IsValid());
    }
    MeshData cube = BuiltinAssets::CreateCube();
    EXPECT_EQ(cube.vertices.size(), 24u);
    EXPECT_EQ(cube.indices.size(), 36u);
    EXPECT_NEAR(cube.bounds.Size().x, 1.0f, 1.0e-5f);
    EXPECT_NEAR(BuiltinAssets::CreatePlane().bounds.Size().x, 10.0f, 1.0e-4f);
}

TEST(RenderingAssetTests, BuiltinAssetsLoadThroughDatabase)
{
    auto cube = Assets::AssetDatabase::Get().Load<ModelAsset>(BuiltinAssets::Cube());
    ASSERT_NE(cube, nullptr);
    EXPECT_EQ(cube->GetMeshes().size(), 1u);
    EXPECT_EQ(cube->GetName(), "Cube");
    EXPECT_NE(Assets::AssetDatabase::Get().Load<MaterialAsset>(BuiltinAssets::DefaultMaterial()), nullptr);
}

TEST(RenderingAssetTests, ImportsFbxIntoLeftHandedSpace)
{
    RenderingProject project;
    std::filesystem::path source = std::filesystem::path(BRIGHT_UFBX_DATA_DIR) / "blender_272_cube_7400_binary.fbx";
    ASSERT_TRUE(std::filesystem::exists(source));
    std::filesystem::copy_file(source, project.Assets() / "Cube.fbx");
    project.Open();
    const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecordByPath("Cube.fbx");
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->type, Assets::AssetType::Model);
    auto model = Assets::AssetDatabase::Get().Load<ModelAsset>(record->guid);
    ASSERT_NE(model, nullptr);
    ASSERT_FALSE(model->GetMeshes().empty());
    const MeshData& mesh = model->GetMeshes()[0];
    EXPECT_EQ(mesh.indices.size() % 3, 0u);
    EXPECT_GE(mesh.vertices.size(), 8u);
    ExpectOutwardWinding(mesh);
    EXPECT_FALSE(model->GetNodes().empty());
}

TEST(RenderingAssetTests, ImportsPngWithMipChain)
{
    RenderingProject project;
    std::vector<uint8_t> pixels(8 * 4 * 4, 255);
    for (int i = 0; i < 8 * 4; ++i)
    {
        pixels[static_cast<size_t>(i) * 4 + 0] = i % 2 ? 255 : 0;
    }
    std::string path = (project.Assets() / "Checker.png").string();
    ASSERT_NE(stbi_write_png(path.c_str(), 8, 4, 4, pixels.data(), 8 * 4), 0);
    project.Open();
    const Assets::AssetRecord* record = Assets::AssetDatabase::Get().FindRecordByPath("Checker.png");
    ASSERT_NE(record, nullptr);
    auto texture = Assets::AssetDatabase::Get().Load<TextureAsset>(record->guid);
    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->GetWidth(), 8u);
    EXPECT_EQ(texture->GetHeight(), 4u);
    EXPECT_EQ(texture->GetMips().size(), 4u);
    EXPECT_EQ(texture->GetPixelFormat(), TexturePixelFormat::RGBA8Srgb);
    const TextureMip& last = texture->GetMips().back();
    EXPECT_EQ(last.width, 1u);
    EXPECT_GT(last.pixels[0], 160);
    EXPECT_LT(last.pixels[0], 200);
}

TEST(RenderingAssetTests, MaterialsRoundTripThroughJson)
{
    RenderingProject project;
    project.Open();
    MaterialAsset source;
    source.m_baseColor = { 0.25f, 0.5f, 0.75f, 1.0f };
    source.m_roughness = 0.1f;
    source.m_blendMode = BlendMode::Transparent;
    Serialization::JsonDocument document;
    Serialization::SerializeProperties(source, document, document.GetAllocator());
    Core::Guid guid = Assets::AssetDatabase::Get().CreateJsonAsset("Glass.material", document);
    auto material = Assets::AssetDatabase::Get().Load<MaterialAsset>(guid);
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(material->m_baseColor, source.m_baseColor);
    EXPECT_EQ(material->m_roughness, 0.1f);
    EXPECT_EQ(material->m_blendMode, BlendMode::Transparent);
}
