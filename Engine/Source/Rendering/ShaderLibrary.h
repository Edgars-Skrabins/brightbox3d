#pragma once

#include "Rendering/ShaderProgram.h"
#include "RHI/Device.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace BrightBox3D::Rendering
{
    using ShaderDefines = std::vector<std::pair<std::string, std::string>>;

    class ShaderLibrary
    {
    public:
        ShaderLibrary(RHI::Device& device, std::filesystem::path directory);

        RHI::Shader* Get(ShaderProgram program, RHI::ShaderStage stage);
        RHI::Shader* Get(const std::string& file, const std::string& entryPoint, RHI::ShaderStage stage, const ShaderDefines& defines = {});
        RHI::Pipeline* GetPipeline(const PipelineKey& key, const RHI::PipelineDesc& desc);
        void Clear();
        const std::filesystem::path& GetDirectory() const { return m_directory; }
        uint64_t GetRevision() const { return m_revision; }

    private:
        struct ShaderKey
        {
            std::string file;
            std::string entryPoint;
            RHI::ShaderStage stage = RHI::ShaderStage::Vertex;
            ShaderDefines defines;

            bool operator==(const ShaderKey& other) const = default;
        };

        struct ShaderKeyHash
        {
            size_t operator()(const ShaderKey& key) const;
        };

        RHI::Device& m_device;
        std::filesystem::path m_directory;
        std::unordered_map<ShaderKey, std::unique_ptr<RHI::Shader>, ShaderKeyHash> m_shaders;
        std::unordered_map<PipelineKey, std::unique_ptr<RHI::Pipeline>, PipelineKeyHash> m_pipelines;
        std::unordered_map<std::string, std::string> m_sources;
        uint64_t m_revision = 0;
    };
}
