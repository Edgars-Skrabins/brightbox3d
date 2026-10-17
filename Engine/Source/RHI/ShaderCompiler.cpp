#include "RHI/ShaderCompiler.h"

#include "Core/EngineText.h"
#include "Core/FileSystem.h"
#include "Core/StringUtils.h"

#include <set>
#include <sstream>

#if defined(__APPLE__)
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_msl.hpp>

#include <mutex>
#endif

namespace BrightBox3D::RHI
{
    namespace
    {
        constexpr uint32_t TextureBindingShift = 32;
        constexpr uint32_t SamplerBindingShift = 64;
        constexpr int MaxIncludeDepth = 32;
        constexpr const char* IncludeDirective = "#include";
        constexpr const char* PragmaOnceDirective = "#pragma once";

        Core::Result<void> PreprocessRecursive(const std::string& source, const std::filesystem::path& directory, std::set<std::string>& included, std::string& output, int depth)
        {
            if (depth > MaxIncludeDepth)
            {
                return Core::Error(Core::EngineText::ShaderIncludeDepth);
            }
            std::istringstream stream(source);
            std::string line;
            while (std::getline(stream, line))
            {
                std::string trimmed = Core::Trim(line);
                if (Core::StartsWith(trimmed, IncludeDirective))
                {
                    size_t open = trimmed.find('"');
                    size_t close = trimmed.rfind('"');
                    if (open == std::string::npos || close <= open)
                    {
                        return Core::Error(Core::EngineText::ShaderMalformedInclude, trimmed);
                    }
                    std::string name = trimmed.substr(open + 1, close - open - 1);
                    std::filesystem::path path = directory / name;
                    std::string key = path.lexically_normal().generic_string();
                    if (included.count(key))
                    {
                        continue;
                    }
                    included.insert(key);
                    auto text = Core::FileSystem::ReadText(path);
                    if (!text)
                    {
                        return Core::Error(Core::EngineText::ShaderIncludeMissing, key);
                    }
                    if (Core::Result<void> nested = PreprocessRecursive(*text, path.parent_path(), included, output, depth + 1); !nested)
                    {
                        return nested;
                    }
                    output += "\n";
                    continue;
                }
                if (trimmed == PragmaOnceDirective)
                {
                    output += "\n";
                    continue;
                }
                output += line;
                output += "\n";
            }
            return {};
        }
    }

    Core::Result<std::string> ShaderCompiler::Preprocess(const std::filesystem::path& file)
    {
        auto text = Core::FileSystem::ReadText(file);
        if (!text)
        {
            return Core::Error(Core::EngineText::ShaderFileMissing, file.generic_string());
        }
        return PreprocessSource(*text, file.parent_path());
    }

    Core::Result<std::string> ShaderCompiler::PreprocessSource(const std::string& source, const std::filesystem::path& includeDirectory)
    {
        std::set<std::string> included;
        std::string output;
        if (Core::Result<void> processed = PreprocessRecursive(source, includeDirectory, included, output, 0); !processed)
        {
            return processed.GetError();
        }
        return output;
    }

    std::string ShaderCompiler::BuildDefinePreamble(const std::vector<std::pair<std::string, std::string>>& defines)
    {
        std::string preamble;
        for (const auto& [name, value] : defines)
        {
            preamble += "#define " + name + " " + (value.empty() ? std::string("1") : value) + "\n";
        }
        return preamble;
    }

#if defined(__APPLE__)
    Core::Result<MetalShaderSource> ShaderCompiler::CompileToMetal(const ShaderDesc& desc)
    {
        static std::once_flag initializeFlag;
        std::call_once(initializeFlag, [] { glslang::InitializeProcess(); });

        EShLanguage language = desc.stage == ShaderStage::Vertex ? EShLangVertex : EShLangFragment;
        glslang::TShader shader(language);
        std::string source = "#pragma pack_matrix(row_major)\n" + desc.source;
        const char* sources[] = { source.c_str() };
        shader.setStrings(sources, 1);
        shader.setEntryPoint(desc.entryPoint.c_str());
        shader.setSourceEntryPoint(desc.entryPoint.c_str());
        std::string preamble = BuildDefinePreamble(desc.defines);
        shader.setPreamble(preamble.c_str());
        shader.setEnvInput(glslang::EShSourceHlsl, language, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
        shader.setEnvTargetHlslFunctionality1();
        shader.setHlslIoMapping(true);
        shader.setShiftBinding(glslang::EResUbo, 0);
        shader.setShiftBinding(glslang::EResTexture, TextureBindingShift);
        shader.setShiftBinding(glslang::EResSampler, SamplerBindingShift);

        EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl | EShMsgHlslOffsets);
        if (!shader.parse(GetDefaultResources(), 100, false, messages))
        {
            return Core::Error(Core::EngineText::GlslangError, std::string(shader.getInfoLog()) + shader.getInfoDebugLog());
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages))
        {
            return Core::Error(Core::EngineText::GlslangError, std::string(program.getInfoLog()) + program.getInfoDebugLog());
        }

        std::vector<uint32_t> spirv;
        glslang::SpvOptions spvOptions;
        spvOptions.generateDebugInfo = false;
        spvOptions.disableOptimizer = true;
        glslang::GlslangToSpv(*program.getIntermediate(language), spirv, &spvOptions);

        try
        {
            spirv_cross::CompilerMSL compiler(std::move(spirv));
            spirv_cross::CompilerMSL::Options options = compiler.get_msl_options();
            options.platform = spirv_cross::CompilerMSL::Options::macOS;
            options.set_msl_version(2, 3);
            options.enable_decoration_binding = true;
            options.pad_fragment_output_components = true;
            compiler.set_msl_options(options);

            spirv_cross::ShaderResources resources = compiler.get_shader_resources();
            for (const spirv_cross::Resource& resource : resources.separate_images)
            {
                uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                compiler.set_decoration(resource.id, spv::DecorationBinding, binding >= TextureBindingShift ? binding - TextureBindingShift : binding);
            }
            for (const spirv_cross::Resource& resource : resources.separate_samplers)
            {
                uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                compiler.set_decoration(resource.id, spv::DecorationBinding, binding >= SamplerBindingShift ? binding - SamplerBindingShift : binding);
            }

            MetalShaderSource metal;
            metal.source = compiler.compile();
            spv::ExecutionModel model = desc.stage == ShaderStage::Vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
            metal.entryPoint = compiler.get_cleansed_entry_point_name(desc.entryPoint, model);
            return metal;
        }
        catch (const std::exception& exception)
        {
            return Core::Error(Core::EngineText::SpirvCrossError, exception.what());
        }
    }
#else
    Core::Result<MetalShaderSource> ShaderCompiler::CompileToMetal(const ShaderDesc& desc)
    {
        return Core::Error(Core::EngineText::MetalOnlyOnApple);
    }
#endif
}
