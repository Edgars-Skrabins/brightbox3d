#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BrightBox3D::RHI
{
    enum class Backend
    {
        Metal,
        DX11
    };

    enum class Format
    {
        Unknown,
        R8Unorm,
        RG8Unorm,
        RGBA8Unorm,
        RGBA8Srgb,
        BGRA8Unorm,
        BGRA8Srgb,
        R16Float,
        RG16Float,
        RGBA16Float,
        R32Float,
        RG32Float,
        RGB32Float,
        RGBA32Float,
        R32Uint,
        RG11B10Float,
        RGB10A2Unorm,
        Depth32Float,
        Depth24Stencil8
    };

    bool IsDepthFormat(Format format);
    uint32_t FormatBytesPerPixel(Format format);

    enum TextureUsage : uint32_t
    {
        TextureUsageSampled = 1u << 0,
        TextureUsageRenderTarget = 1u << 1,
        TextureUsageDepthStencil = 1u << 2
    };

    enum class TextureType
    {
        Texture2D,
        TextureCube,
        Texture2DArray
    };

    struct TextureDesc
    {
        TextureType type = TextureType::Texture2D;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t layers = 1;
        uint32_t mipLevels = 1;
        Format format = Format::RGBA8Unorm;
        uint32_t usage = TextureUsageSampled;
        std::string debugName;
    };

    enum class BufferUsage
    {
        Vertex,
        Index,
        Constant
    };

    struct BufferDesc
    {
        size_t size = 0;
        BufferUsage usage = BufferUsage::Vertex;
        bool dynamic = false;
        std::string debugName;
    };

    enum class IndexFormat
    {
        UInt16,
        UInt32
    };

    enum class Filter
    {
        Point,
        Linear,
        Anisotropic
    };

    enum class AddressMode
    {
        Wrap,
        Clamp,
        Mirror,
        Border
    };

    enum class CompareFunction
    {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always
    };

    struct SamplerDesc
    {
        Filter filter = Filter::Linear;
        bool mipLinear = true;
        AddressMode addressU = AddressMode::Wrap;
        AddressMode addressV = AddressMode::Wrap;
        AddressMode addressW = AddressMode::Wrap;
        bool comparison = false;
        CompareFunction compare = CompareFunction::LessEqual;
        uint32_t maxAnisotropy = 8;
        float mipLodBias = 0.0f;
        bool borderWhite = false;
    };

    enum class ShaderStage
    {
        Vertex,
        Pixel
    };

    struct ShaderDesc
    {
        std::string source;
        std::string entryPoint = "main";
        ShaderStage stage = ShaderStage::Vertex;
        std::vector<std::pair<std::string, std::string>> defines;
        std::string debugName;
    };

    struct VertexAttribute
    {
        std::string semantic;
        uint32_t semanticIndex = 0;
        Format format = Format::RGB32Float;
        uint32_t offset = 0;
        uint32_t bufferSlot = 0;
    };

    struct VertexLayout
    {
        std::vector<VertexAttribute> attributes;
        std::array<uint32_t, 4> strides{};
        std::array<bool, 4> perInstance{};
    };

    enum class PrimitiveTopology
    {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList
    };

    enum class CullMode
    {
        None,
        Front,
        Back
    };

    struct RasterizerState
    {
        CullMode cullMode = CullMode::Back;
        bool wireframe = false;
        bool frontCounterClockwise = false;
        int32_t depthBias = 0;
        float slopeScaledDepthBias = 0.0f;
        float depthBiasClamp = 0.0f;
        bool depthClip = true;
        bool scissor = false;
    };

    struct DepthStencilState
    {
        bool depthTest = true;
        bool depthWrite = true;
        CompareFunction depthCompare = CompareFunction::LessEqual;
    };

    enum class BlendFactor
    {
        Zero,
        One,
        SourceColor,
        InverseSourceColor,
        SourceAlpha,
        InverseSourceAlpha,
        DestinationColor,
        InverseDestinationColor,
        DestinationAlpha,
        InverseDestinationAlpha
    };

    enum class BlendOperation
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    enum ColorWrite : uint8_t
    {
        ColorWriteRed = 1,
        ColorWriteGreen = 2,
        ColorWriteBlue = 4,
        ColorWriteAlpha = 8,
        ColorWriteAll = 15
    };

    struct BlendState
    {
        bool enabled = false;
        BlendFactor sourceColor = BlendFactor::One;
        BlendFactor destinationColor = BlendFactor::Zero;
        BlendOperation colorOperation = BlendOperation::Add;
        BlendFactor sourceAlpha = BlendFactor::One;
        BlendFactor destinationAlpha = BlendFactor::Zero;
        BlendOperation alphaOperation = BlendOperation::Add;
        uint8_t writeMask = ColorWriteAll;

        static BlendState AlphaBlend()
        {
            BlendState state;
            state.enabled = true;
            state.sourceColor = BlendFactor::SourceAlpha;
            state.destinationColor = BlendFactor::InverseSourceAlpha;
            state.sourceAlpha = BlendFactor::One;
            state.destinationAlpha = BlendFactor::InverseSourceAlpha;
            return state;
        }

        static BlendState Additive()
        {
            BlendState state;
            state.enabled = true;
            state.sourceColor = BlendFactor::One;
            state.destinationColor = BlendFactor::One;
            state.sourceAlpha = BlendFactor::One;
            state.destinationAlpha = BlendFactor::One;
            return state;
        }

        static BlendState Premultiplied()
        {
            BlendState state;
            state.enabled = true;
            state.sourceColor = BlendFactor::One;
            state.destinationColor = BlendFactor::InverseSourceAlpha;
            state.sourceAlpha = BlendFactor::One;
            state.destinationAlpha = BlendFactor::InverseSourceAlpha;
            return state;
        }
    };

    inline constexpr uint32_t MaxColorTargets = 8;
    inline constexpr uint32_t MaxConstantSlots = 14;
    inline constexpr uint32_t MaxTextureSlots = 16;
    inline constexpr uint32_t MaxSamplerSlots = 16;
    inline constexpr uint32_t MaxVertexBuffers = 4;

    class Shader;

    struct PipelineDesc
    {
        Shader* vertexShader = nullptr;
        Shader* pixelShader = nullptr;
        VertexLayout vertexLayout;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;
        RasterizerState rasterizer;
        DepthStencilState depthStencil;
        std::array<BlendState, MaxColorTargets> blend{};
        std::vector<Format> colorFormats;
        Format depthFormat = Format::Unknown;
        std::string debugName;
    };

    enum class LoadAction
    {
        Load,
        Clear,
        DontCare
    };

    class Texture;

    struct ColorAttachment
    {
        Texture* texture = nullptr;
        uint32_t mipLevel = 0;
        uint32_t layer = 0;
        LoadAction load = LoadAction::Clear;
        std::array<float, 4> clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct DepthAttachment
    {
        Texture* texture = nullptr;
        uint32_t layer = 0;
        LoadAction load = LoadAction::Clear;
        float clearDepth = 1.0f;
    };

    struct RenderPassDesc
    {
        std::vector<ColorAttachment> colors;
        DepthAttachment depth;
        std::string debugName;
    };

    struct Viewport
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float minDepth = 0.0f;
        float maxDepth = 1.0f;
    };

    struct ScissorRect
    {
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = 0;
        int32_t height = 0;
    };
}
