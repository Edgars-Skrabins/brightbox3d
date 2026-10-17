#include "Rendering/ShaderLibrary.h"

#include "Core/EngineText.h"
#include "Core/Log.h"
#include "RHI/ShaderCompiler.h"

namespace BrightBox3D::Rendering
{
    ShaderLibrary::ShaderLibrary(RHI::Device& device, std::filesystem::path directory) : m_device(device), m_directory(std::move(directory))
    {
    }

    size_t ShaderLibrary::ShaderKeyHash::operator()(const ShaderKey& key) const
    {
        std::hash<std::string> hashText;
        size_t hash = hashText(key.file);
        hash = hash * 31 + hashText(key.entryPoint);
        hash = hash * 31 + static_cast<size_t>(key.stage);
        for (const auto& [name, value] : key.defines)
        {
            hash = hash * 31 + hashText(name);
            hash = hash * 31 + hashText(value);
        }
        return hash;
    }

    RHI::Shader* ShaderLibrary::Get(ShaderProgram program, RHI::ShaderStage stage)
    {
        const ShaderProgramInfo& info = GetShaderProgramInfo(program);
        return Get(info.file, stage == RHI::ShaderStage::Vertex ? info.vertexEntry : info.pixelEntry, stage);
    }

    RHI::Shader* ShaderLibrary::Get(const std::string& file, const std::string& entryPoint, RHI::ShaderStage stage, const ShaderDefines& defines)
    {
        ShaderKey key{ file, entryPoint, stage, defines };
        auto it = m_shaders.find(key);
        if (it != m_shaders.end())
        {
            return it->second.get();
        }

        auto source = m_sources.find(file);
        if (source == m_sources.end())
        {
            Core::Result<std::string> text = RHI::ShaderCompiler::Preprocess(m_directory / file);
            if (!text)
            {
                Core::Log::Error(Core::Text(Core::EngineText::ShaderLoadFailed, file, text.GetError().GetText()));
                m_shaders[key] = nullptr;
                return nullptr;
            }
            source = m_sources.emplace(file, std::move(*text)).first;
        }

        RHI::ShaderDesc desc;
        desc.source = source->second;
        desc.entryPoint = entryPoint;
        desc.stage = stage;
        desc.defines = defines;
        desc.debugName = file + ":" + entryPoint;
        Core::Result<std::unique_ptr<RHI::Shader>> shader = m_device.CreateShader(desc);
        if (!shader)
        {
            Core::Log::Write(Core::LogLevel::Error, Core::Text(Core::EngineText::ShaderCompileFailed, desc.debugName, shader.GetError().GetText()), Core::LogSource::Engine, Core::LogCategory::Shader, (m_directory / file).generic_string(), 0);
            m_shaders[key] = nullptr;
            return nullptr;
        }
        RHI::Shader* raw = shader.GetValue().get();
        m_shaders[key] = std::move(*shader);
        return raw;
    }

    RHI::Pipeline* ShaderLibrary::GetPipeline(const PipelineKey& key, const RHI::PipelineDesc& desc)
    {
        auto it = m_pipelines.find(key);
        if (it != m_pipelines.end())
        {
            return it->second.get();
        }
        if (!desc.vertexShader || !desc.pixelShader)
        {
            m_pipelines[key] = nullptr;
            return nullptr;
        }
        RHI::PipelineDesc named = desc;
        named.debugName = GetShaderProgramInfo(key.program).name;
        Core::Result<std::unique_ptr<RHI::Pipeline>> pipeline = m_device.CreatePipeline(named);
        if (!pipeline)
        {
            Core::Log::Error(Core::Text(Core::EngineText::PipelineFailed, named.debugName, pipeline.GetError().GetText()));
            m_pipelines[key] = nullptr;
            return nullptr;
        }
        RHI::Pipeline* raw = pipeline.GetValue().get();
        m_pipelines[key] = std::move(*pipeline);
        return raw;
    }

    void ShaderLibrary::Clear()
    {
        m_pipelines.clear();
        m_shaders.clear();
        m_sources.clear();
        ++m_revision;
    }
}
