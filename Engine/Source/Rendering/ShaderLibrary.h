#pragma once

#include "RHI/Device.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace BrightBox3D::Rendering
{
    class ShaderLibrary
    {
    public:
        ShaderLibrary(RHI::Device& device, std::filesystem::path directory);

        RHI::Shader* Get(const std::string& file, const std::string& entryPoint, RHI::ShaderStage stage, const std::vector<std::pair<std::string, std::string>>& defines = {});
        RHI::Pipeline* GetPipeline(const std::string& key, const RHI::PipelineDesc& desc);
        void Clear();
        const std::filesystem::path& GetDirectory() const { return m_directory; }
        uint64_t GetRevision() const { return m_revision; }

    private:
        RHI::Device& m_device;
        std::filesystem::path m_directory;
        std::unordered_map<std::string, std::unique_ptr<RHI::Shader>> m_shaders;
        std::unordered_map<std::string, std::unique_ptr<RHI::Pipeline>> m_pipelines;
        std::unordered_map<std::string, std::string> m_sources;
        uint64_t m_revision = 0;
    };
}
