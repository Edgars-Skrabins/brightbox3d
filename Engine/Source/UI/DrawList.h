#pragma once

#include "Math/Color.h"
#include "RHI/Types.h"

#include <array>
#include <cstdint>
#include <vector>

namespace BrightBox3D::RHI
{
    class Texture;
}

namespace BrightBox3D::UI
{
    enum class DrawMode : uint32_t
    {
        Rect = 0,
        Ring = 1,
        Text = 2,
        SrgbImage = 3,
        LinearImage = 4
    };

    struct UIVertex
    {
        float position[2];
        float uv[2];
        float local[2];
        float halfSize[2];
        float params[4];
        uint32_t color;
    };

    struct UIBatch
    {
        RHI::Texture* texture = nullptr;
        bool clipped = false;
        float clip[4] = {};
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
    };

    enum class ImageEncoding
    {
        Linear,
        Srgb
    };

    class DrawList
    {
    public:
        void Clear();
        void PushClip(float x, float y, float width, float height);
        void PopClip();
        void AddRect(float x, float y, float width, float height, const Math::Color& color, float radius);
        void AddBorder(float x, float y, float width, float height, const Math::Color& color, float radius, float thickness);
        void AddImage(float x, float y, float width, float height, RHI::Texture* texture, ImageEncoding encoding, const Math::Color& tint, float radius);
        void AddGlyph(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, RHI::Texture* texture, const Math::Color& color, float unitRangeX, float unitRangeY);

        const std::vector<UIVertex>& GetVertices() const { return m_vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_indices; }
        const std::vector<UIBatch>& GetBatches() const { return m_batches; }

    private:
        void AddQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, RHI::Texture* texture, const Math::Color& color, DrawMode mode, float a, float b, float c);
        UIBatch& CurrentBatch(RHI::Texture* texture);

        std::vector<UIVertex> m_vertices;
        std::vector<uint32_t> m_indices;
        std::vector<UIBatch> m_batches;
        std::vector<std::array<float, 4>> m_clipStack;
    };
}
