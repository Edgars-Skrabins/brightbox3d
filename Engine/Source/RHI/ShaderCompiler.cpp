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

        bool PreprocessRecursive(const std::string& source, const std::filesystem::path& directory, std::set<std::string>& included, std::string& output, std::string& error, int depth)
        {
            if (depth > MaxIncludeDepth)
            {
                error = Core::Text(Core::EngineText::ShaderIncludeDepth);
                return false;
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
                        error = Core::Text(Core::EngineText::ShaderMalformedInclude, trimmed);
                        return false;
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
                        error = Core::Text(Core::EngineText::ShaderIncludeMissing, key);
                        return false;
                    }
                    if (!PreprocessRecursive(*text, path.parent_path(), included, output, error, depth + 1))
                    {
                        return false;
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
            return true;
        }
    }

    bool ShaderCompiler::Preprocess(const std::filesystem::path& file, std::string& output, std::string& error)
    {
        auto text = Core::FileSystem::ReadText(file);
        if (!text)
        {
            error = Core::Text(Core::EngineText::ShaderFileMissing, file.generic_string());
            return false;
        }
        return PreprocessSource(*text, file.parent_path(), output, error);
    }

    bool ShaderCompiler::PreprocessSource(const std::string& source, const std::filesystem::path& includeDirectory, std::string& output, std::string& error)
    {
        std::set<std::string> included;
        output.clear();
        return PreprocessRecursive(source, includeDirectory, included, output, error, 0);
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
    ShaderCompileResult ShaderCompiler::CompileToMetal(const ShaderDesc& desc)
    {
        static std::once_flag initializeFlag;
        std::call_once(initializeFlag, [] { glslang::InitializeProcess(); });

        ShaderCompileResult result;
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
            result.error = std::string(shader.getInfoLog()) + shader.getInfoDebugLog();
            return result;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages))
        {
            result.error = std::string(program.getInfoLog()) + program.getInfoDebugLog();
            return result;
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

            result.metalSource = compiler.compile();
            spv::ExecutionModel model = desc.stage == ShaderStage::Vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
            result.metalEntryPoint = compiler.get_cleansed_entry_point_name(desc.entryPoint, model);
            result.success = true;
        }
        catch (const std::exception& exception)
        {
            result.error = Core::Text(Core::EngineText::SpirvCrossError, exception.what());
        }
        return result;
    }
#else
    ShaderCompileResult ShaderCompiler::CompileToMetal(const ShaderDesc& desc)
    {
        ShaderCompileResult result;
        result.error = Core::Text(Core::EngineText::MetalOnlyOnApple);
        return result;
    }
#endif
}
