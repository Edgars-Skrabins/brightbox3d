#include "UI/DocumentPainter.h"

#include "Core/Utf8.h"
#include "Math/MathUtils.h"
#include "Rendering/GpuResourceCache.h"
#include "UI/DrawList.h"
#include "UI/Element.h"
#include "UI/Font.h"

#include <yoga/Yoga.h>

#include <algorithm>
#include <vector>

namespace BrightBox3D::UI::DocumentPainter
{
    namespace
    {
        constexpr float SliderKnobGrowth = 4.0f;
        constexpr float ToggleMarkInset = 0.25f;
        constexpr float PlaceholderAlpha = 0.45f;
        constexpr float CaretOffset = 1.0f;
        constexpr float CaretWidth = 1.5f;
        constexpr float WrapWidthSlack = 0.5f;
        constexpr uint32_t MissingGlyph = '?';

        struct PaintContext
        {
            DrawList& drawList;
            RHI::Device& device;
            Rendering::GpuResourceCache& resources;
        };

        struct ElementBox
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            float paddingLeft = 0.0f;
            float paddingTop = 0.0f;
            float innerWidth = 0.0f;
            float opacity = 1.0f;

            Math::Color Faded(Math::Color color) const
            {
                color.a *= opacity;
                return color;
            }
        };

        ElementBox MeasureBox(const Element& element, float opacity)
        {
            const ComputedStyle& style = element.GetStyle();
            ElementBox box;
            box.x = element.GetX();
            box.y = element.GetY();
            box.width = element.GetWidth();
            box.height = element.GetHeight();
            box.paddingLeft = YGNodeLayoutGetPadding(element.GetNode(), YGEdgeLeft) + style.borderWidth;
            float paddingRight = YGNodeLayoutGetPadding(element.GetNode(), YGEdgeRight) + style.borderWidth;
            box.paddingTop = YGNodeLayoutGetPadding(element.GetNode(), YGEdgeTop) + style.borderWidth;
            box.innerWidth = std::max(0.0f, box.width - box.paddingLeft - paddingRight);
            box.opacity = opacity;
            return box;
        }

        float FillFraction(const Element& element)
        {
            float range = element.GetMaximum() - element.GetMinimum();
            return range != 0.0f ? Math::Clamp01((element.GetValue() - element.GetMinimum()) / range) : 0.0f;
        }

        void DrawWidget(const Element& element, const ElementBox& box, PaintContext& context)
        {
            const ComputedStyle& style = element.GetStyle();
            switch (element.GetElementType())
            {
            case ElementType::ProgressBar:
                context.drawList.AddRect(box.x, box.y, box.width * FillFraction(element), box.height, box.Faded(style.fillColor), style.borderRadius);
                break;
            case ElementType::Slider:
            {
                float t = FillFraction(element);
                context.drawList.AddRect(box.x, box.y, box.width * t, box.height, box.Faded(style.fillColor), style.borderRadius);
                float knob = box.height + SliderKnobGrowth;
                context.drawList.AddRect(box.x + box.width * t - knob * 0.5f, box.y + box.height * 0.5f - knob * 0.5f, knob, knob, box.Faded(style.color), knob * 0.5f);
                break;
            }
            case ElementType::Toggle:
                if (element.GetBool())
                {
                    float inset = std::min(box.width, box.height) * ToggleMarkInset;
                    context.drawList.AddRect(box.x + inset, box.y + inset, box.width - inset * 2.0f, box.height - inset * 2.0f, box.Faded(style.color), style.borderRadius * 0.5f);
                }
                break;
            case ElementType::Image:
                if (Rendering::GpuTexture* texture = context.resources.GetTexture(element.GetImage()))
                {
                    context.drawList.AddImage(box.x, box.y, box.width, box.height, texture->texture.get(), texture->texture->GetFormat() == RHI::Format::RGBA8Srgb ? ImageEncoding::Srgb : ImageEncoding::Linear, box.Faded(style.color), style.borderRadius);
                }
                break;
            default:
                break;
            }
        }

        bool ShowsCaret(const Element& element)
        {
            return element.GetElementType() == ElementType::TextField && element.IsFocused();
        }

        void DrawCaret(float x, float y, float lineHeight, const ElementBox& box, const ComputedStyle& style, PaintContext& context)
        {
            context.drawList.AddRect(x + CaretOffset, y, CaretWidth, lineHeight, box.Faded(style.color), 0.0f);
        }

        float DrawLine(const TextLine& line, float penX, float top, float size, const FontData& data, RHI::Texture* atlas, const Math::Color& color, PaintContext& context)
        {
            float unitX = data.pixelRange / static_cast<float>(data.atlasWidth);
            float unitY = data.pixelRange / static_cast<float>(data.atlasHeight);
            float baseline = top + data.ascender * size;
            uint32_t previous = 0;
            for (uint32_t codepoint : Core::Utf8::Decode(line.text))
            {
                const FontGlyph* glyph = data.FindGlyph(codepoint);
                if (!glyph)
                {
                    glyph = data.FindGlyph(MissingGlyph);
                }
                if (!glyph)
                {
                    continue;
                }
                if (previous)
                {
                    penX += data.GetKerning(previous, codepoint) * size;
                }
                if (glyph->planeRight > glyph->planeLeft)
                {
                    context.drawList.AddGlyph(penX + glyph->planeLeft * size, baseline - glyph->planeTop * size, penX + glyph->planeRight * size, baseline - glyph->planeBottom * size, glyph->atlasLeft, glyph->atlasTop, glyph->atlasRight, glyph->atlasBottom, atlas, color, unitX, unitY);
                }
                penX += glyph->advance * size;
                previous = codepoint;
            }
            return penX;
        }

        float AlignedPenX(const TextLine& line, const ElementBox& box, TextAlign align)
        {
            float penX = box.x + box.paddingLeft;
            if (align == TextAlign::Center)
            {
                penX += (box.innerWidth - line.width) * 0.5f;
            }
            else if (align == TextAlign::Right)
            {
                penX += box.innerWidth - line.width;
            }
            return penX;
        }

        void DrawElementText(const Element& element, const std::string& text, const ElementBox& box, PaintContext& context)
        {
            const ComputedStyle& style = element.GetStyle();
            Font* font = FontLibrary::Get().Get(style.fontFamily);
            RHI::Texture* atlas = font ? font->GetTexture(context.device) : nullptr;
            if (!font || !atlas)
            {
                return;
            }
            const FontData& data = font->GetData();
            float size = style.fontSize;
            std::vector<TextLine> lines = font->LayoutLines(text, size, box.innerWidth + WrapWidthSlack);
            float lineHeight = font->LineHeight(size);
            float contentHeight = lineHeight * static_cast<float>(lines.size());
            float top = element.GetElementType() == ElementType::Label ? box.y + box.paddingTop : box.y + (box.height - contentHeight) * 0.5f;
            Math::Color color = box.Faded(style.color);
            if (element.GetElementType() == ElementType::TextField && element.GetText().empty())
            {
                color.a *= PlaceholderAlpha;
            }
            float caretX = box.x + box.paddingLeft;
            float caretY = top;
            for (const TextLine& line : lines)
            {
                caretX = DrawLine(line, AlignedPenX(line, box, style.textAlign), top, size, data, atlas, color, context);
                caretY = top;
                top += lineHeight;
            }
            if (ShowsCaret(element))
            {
                if (element.GetText().empty())
                {
                    caretX = box.x + box.paddingLeft;
                }
                DrawCaret(caretX, caretY, lineHeight, box, style, context);
            }
        }

        void DrawEmptyCaret(const Element& element, const ElementBox& box, PaintContext& context)
        {
            const ComputedStyle& style = element.GetStyle();
            Font* font = FontLibrary::Get().Get(style.fontFamily);
            float lineHeight = font ? font->LineHeight(style.fontSize) : style.fontSize;
            DrawCaret(box.x + box.paddingLeft, box.y + (box.height - lineHeight) * 0.5f, lineHeight, box, style, context);
        }

        void DrawElement(const Element& element, PaintContext& context, float parentOpacity)
        {
            const ComputedStyle& style = element.GetStyle();
            if (style.IsHidden())
            {
                return;
            }
            ElementBox box = MeasureBox(element, parentOpacity * style.opacity);
            context.drawList.AddRect(box.x, box.y, box.width, box.height, box.Faded(style.backgroundColor), style.borderRadius);
            DrawWidget(element, box, context);

            const std::string& text = element.GetDisplayText();
            if (!text.empty() && element.ShowsText())
            {
                DrawElementText(element, text, box, context);
            }
            else if (ShowsCaret(element))
            {
                DrawEmptyCaret(element, box, context);
            }

            context.drawList.AddBorder(box.x, box.y, box.width, box.height, box.Faded(style.borderColor), style.borderRadius, style.borderWidth);

            bool clip = style.overflow == Overflow::Hidden;
            if (clip)
            {
                context.drawList.PushClip(box.x, box.y, box.width, box.height);
            }
            for (const auto& child : element.GetChildren())
            {
                DrawElement(*child, context, box.opacity);
            }
            if (clip)
            {
                context.drawList.PopClip();
            }
        }
    }

    void Draw(const Element& root, DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources)
    {
        PaintContext context{ drawList, device, resources };
        DrawElement(root, context, 1.0f);
    }
}
