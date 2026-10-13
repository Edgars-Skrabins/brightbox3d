#include "UI/DrawList.h"

#include <algorithm>

namespace BrightBox3D::UI
{
    void DrawList::Clear()
    {
        m_vertices.clear();
        m_indices.clear();
        m_batches.clear();
        m_clipStack.clear();
    }

    void DrawList::PushClip(float x, float y, float width, float height)
    {
        std::array<float, 4> clip{ x, y, x + width, y + height };
        if (!m_clipStack.empty())
        {
            const auto& parent = m_clipStack.back();
            clip[0] = std::max(clip[0], parent[0]);
            clip[1] = std::max(clip[1], parent[1]);
            clip[2] = std::min(clip[2], parent[2]);
            clip[3] = std::min(clip[3], parent[3]);
        }
        m_clipStack.push_back(clip);
    }

    void DrawList::PopClip()
    {
        if (!m_clipStack.empty())
        {
            m_clipStack.pop_back();
        }
    }

    UIBatch& DrawList::CurrentBatch(RHI::Texture* texture)
    {
        bool clipped = !m_clipStack.empty();
        if (!m_batches.empty())
        {
            UIBatch& last = m_batches.back();
            bool sameClip = last.clipped == clipped && (!clipped || std::equal(last.clip, last.clip + 4, m_clipStack.back().begin()));
            if (last.texture == texture && sameClip)
            {
                return last;
            }
        }
        UIBatch batch;
        batch.texture = texture;
        batch.clipped = clipped;
        if (clipped)
        {
            std::copy(m_clipStack.back().begin(), m_clipStack.back().end(), batch.clip);
        }
        batch.firstIndex = static_cast<uint32_t>(m_indices.size());
        m_batches.push_back(batch);
        return m_batches.back();
    }

    void DrawList::AddQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, RHI::Texture* texture, const Math::Color& color, DrawMode mode, float a, float b, float c)
    {
        if (color.a <= 0.0f || x1 <= x0 || y1 <= y0)
        {
            return;
        }
        UIBatch& batch = CurrentBatch(texture);
        uint32_t base = static_cast<uint32_t>(m_vertices.size());
        float halfWidth = (x1 - x0) * 0.5f;
        float halfHeight = (y1 - y0) * 0.5f;
        uint32_t packed = color.ToRgba8();
        float corners[4][4] = { { x0, y0, u0, v0 }, { x1, y0, u1, v0 }, { x1, y1, u1, v1 }, { x0, y1, u0, v1 } };
        for (int i = 0; i < 4; ++i)
        {
            UIVertex vertex{};
            vertex.position[0] = corners[i][0];
            vertex.position[1] = corners[i][1];
            vertex.uv[0] = corners[i][2];
            vertex.uv[1] = corners[i][3];
            vertex.local[0] = corners[i][0] - (x0 + halfWidth);
            vertex.local[1] = corners[i][1] - (y0 + halfHeight);
            vertex.halfSize[0] = halfWidth;
            vertex.halfSize[1] = halfHeight;
            vertex.params[0] = static_cast<float>(mode);
            vertex.params[1] = a;
            vertex.params[2] = b;
            vertex.params[3] = c;
            vertex.color = packed;
            m_vertices.push_back(vertex);
        }
        for (uint32_t index : { 0u, 1u, 2u, 0u, 2u, 3u })
        {
            m_indices.push_back(base + index);
        }
        batch.indexCount += 6;
    }

    void DrawList::AddRect(float x, float y, float width, float height, const Math::Color& color, float radius)
    {
        AddQuad(x, y, x + width, y + height, 0, 0, 1, 1, nullptr, color, DrawMode::Rect, std::min(radius, std::min(width, height) * 0.5f), 0.0f, 0.0f);
    }

    void DrawList::AddBorder(float x, float y, float width, float height, const Math::Color& color, float radius, float thickness)
    {
        if (thickness <= 0.0f)
        {
            return;
        }
        AddQuad(x, y, x + width, y + height, 0, 0, 1, 1, nullptr, color, DrawMode::Ring, std::min(radius, std::min(width, height) * 0.5f), thickness, 0.0f);
    }

    void DrawList::AddImage(float x, float y, float width, float height, RHI::Texture* texture, bool srgb, const Math::Color& tint, float radius)
    {
        AddQuad(x, y, x + width, y + height, 0, 0, 1, 1, texture, tint, srgb ? DrawMode::SrgbImage : DrawMode::LinearImage, std::min(radius, std::min(width, height) * 0.5f), 0.0f, 0.0f);
    }

    void DrawList::AddGlyph(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, RHI::Texture* texture, const Math::Color& color, float unitRangeX, float unitRangeY)
    {
        AddQuad(x0, y0, x1, y1, u0, v0, u1, v1, texture, color, DrawMode::Text, unitRangeX, unitRangeY, 0.0f);
    }
}
