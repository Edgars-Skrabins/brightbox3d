#include "UI/LayoutSync.h"

#include "UI/Element.h"
#include "UI/Font.h"

#include <yoga/Yoga.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

namespace BrightBox3D::UI::LayoutSync
{
    namespace
    {
        constexpr YGFlexDirection YogaFlexDirections[] = { YGFlexDirectionColumn, YGFlexDirectionRow, YGFlexDirectionColumnReverse, YGFlexDirectionRowReverse };
        constexpr YGJustify YogaJustify[] = { YGJustifyFlexStart, YGJustifyCenter, YGJustifyFlexEnd, YGJustifySpaceBetween, YGJustifySpaceAround, YGJustifySpaceEvenly };
        constexpr YGAlign YogaAlign[] = { YGAlignAuto, YGAlignFlexStart, YGAlignCenter, YGAlignFlexEnd, YGAlignStretch, YGAlignBaseline, YGAlignSpaceBetween, YGAlignSpaceAround };
        constexpr YGWrap YogaWrap[] = { YGWrapNoWrap, YGWrapWrap };
        constexpr YGPositionType YogaPositionType[] = { YGPositionTypeRelative, YGPositionTypeAbsolute };
        constexpr YGOverflow YogaOverflow[] = { YGOverflowVisible, YGOverflowHidden };
        constexpr YGEdge YogaEdges[] = { YGEdgeTop, YGEdgeRight, YGEdgeBottom, YGEdgeLeft };

        static_assert(std::size(YogaFlexDirections) == static_cast<size_t>(FlexDirection::Count));
        static_assert(std::size(YogaJustify) == static_cast<size_t>(Justify::Count));
        static_assert(std::size(YogaAlign) == static_cast<size_t>(Align::Count));
        static_assert(std::size(YogaWrap) == static_cast<size_t>(FlexWrap::Count));
        static_assert(std::size(YogaPositionType) == static_cast<size_t>(PositionType::Count));
        static_assert(std::size(YogaOverflow) == static_cast<size_t>(Overflow::Count));
        static_assert(std::size(YogaEdges) == BoxSideCount);

        template <typename Yoga, size_t Size, typename Value>
        Yoga ToYoga(const Yoga (&table)[Size], Value value)
        {
            return table[static_cast<size_t>(value)];
        }

        float ConstrainMeasure(float measured, float available, YGMeasureMode mode)
        {
            if (mode == YGMeasureModeExactly)
            {
                return available;
            }
            if (mode == YGMeasureModeAtMost)
            {
                return std::min(measured, available);
            }
            return measured;
        }

        YGSize MeasureText(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode)
        {
            const auto* element = static_cast<const Element*>(YGNodeGetContext(node));
            Font* font = FontLibrary::Get().Get(element->GetStyle().fontFamily);
            float size = element->GetStyle().fontSize;
            if (!font)
            {
                return { 0.0f, size };
            }
            float maxWidth = widthMode == YGMeasureModeUndefined ? 0.0f : width;
            std::vector<TextLine> lines = font->LayoutLines(element->GetDisplayText(), size, maxWidth);
            float measuredWidth = 0.0f;
            for (const TextLine& line : lines)
            {
                measuredWidth = std::max(measuredWidth, line.width);
            }
            float measuredHeight = font->LineHeight(size) * static_cast<float>(std::max<size_t>(lines.size(), 1));
            measuredWidth = ConstrainMeasure(measuredWidth, width, widthMode);
            measuredHeight = ConstrainMeasure(measuredHeight, height, heightMode);
            return { std::ceil(measuredWidth), std::ceil(measuredHeight) };
        }

        bool UsesTextMeasure(const Element& element)
        {
            return element.GetChildren().empty() && element.ShowsText();
        }

        template <typename SetPoints, typename SetPercent, typename SetAuto>
        void SetDimension(YGNodeRef node, const Length& length, SetPoints setPoints, SetPercent setPercent, SetAuto setAuto)
        {
            switch (length.unit)
            {
            case LengthUnit::Pixels:
                setPoints(node, length.value);
                break;
            case LengthUnit::Percent:
                setPercent(node, length.value);
                break;
            default:
                setAuto(node);
                break;
            }
        }

        template <typename SetPoints, typename SetPercent>
        void SetLimit(YGNodeRef node, const Length& length, SetPoints setPoints, SetPercent setPercent)
        {
            if (length.unit == LengthUnit::Pixels)
            {
                setPoints(node, length.value);
            }
            else if (length.unit == LengthUnit::Percent)
            {
                setPercent(node, length.value);
            }
            else
            {
                setPoints(node, YGUndefined);
            }
        }

        float PixelsOr(const Length& length, float fallback)
        {
            return length.unit == LengthUnit::Pixels ? length.value : fallback;
        }

        void SyncFlex(YGNodeRef node, const ComputedStyle& style)
        {
            YGNodeStyleSetDisplay(node, style.IsHidden() ? YGDisplayNone : YGDisplayFlex);
            YGNodeStyleSetFlexDirection(node, ToYoga(YogaFlexDirections, style.flexDirection));
            YGNodeStyleSetJustifyContent(node, ToYoga(YogaJustify, style.justifyContent));
            YGNodeStyleSetAlignItems(node, ToYoga(YogaAlign, style.alignItems));
            YGNodeStyleSetAlignSelf(node, ToYoga(YogaAlign, style.alignSelf));
            YGNodeStyleSetAlignContent(node, ToYoga(YogaAlign, style.alignContent));
            YGNodeStyleSetFlexWrap(node, ToYoga(YogaWrap, style.flexWrap));
            YGNodeStyleSetPositionType(node, ToYoga(YogaPositionType, style.position));
            YGNodeStyleSetOverflow(node, ToYoga(YogaOverflow, style.overflow));
            YGNodeStyleSetFlexGrow(node, style.flexGrow);
            YGNodeStyleSetFlexShrink(node, style.flexShrink);
        }

        void SyncSize(YGNodeRef node, const ComputedStyle& style)
        {
            SetDimension(node, style.width, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto);
            SetDimension(node, style.height, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto);
            SetDimension(node, style.flexBasis, YGNodeStyleSetFlexBasis, YGNodeStyleSetFlexBasisPercent, YGNodeStyleSetFlexBasisAuto);
            SetLimit(node, style.minWidth, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent);
            SetLimit(node, style.minHeight, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent);
            SetLimit(node, style.maxWidth, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent);
            SetLimit(node, style.maxHeight, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent);
        }

        void SyncEdges(YGNodeRef node, const ComputedStyle& style)
        {
            for (size_t side = 0; side < BoxSideCount; ++side)
            {
                YGEdge edge = YogaEdges[side];
                const Length& margin = style.margin[side];
                if (margin.unit == LengthUnit::Auto)
                {
                    YGNodeStyleSetMarginAuto(node, edge);
                }
                else if (margin.unit == LengthUnit::Percent)
                {
                    YGNodeStyleSetMarginPercent(node, edge, margin.value);
                }
                else
                {
                    YGNodeStyleSetMargin(node, edge, PixelsOr(margin, 0.0f));
                }
                const Length& padding = style.padding[side];
                if (padding.unit == LengthUnit::Percent)
                {
                    YGNodeStyleSetPaddingPercent(node, edge, padding.value);
                }
                else
                {
                    YGNodeStyleSetPadding(node, edge, PixelsOr(padding, 0.0f));
                }
                const Length& inset = style.inset[side];
                if (inset.unit == LengthUnit::Percent)
                {
                    YGNodeStyleSetPositionPercent(node, edge, inset.value);
                }
                else
                {
                    YGNodeStyleSetPosition(node, edge, PixelsOr(inset, YGUndefined));
                }
            }
            YGNodeStyleSetBorder(node, YGEdgeAll, style.borderWidth);
            YGNodeStyleSetGap(node, YGGutterAll, style.gap);
        }

        void SyncMeasure(YGNodeRef node, const Element& element)
        {
            if (!UsesTextMeasure(element))
            {
                return;
            }
            if (!YGNodeHasMeasureFunc(node))
            {
                YGNodeSetMeasureFunc(node, &MeasureText);
            }
            YGNodeMarkDirty(node);
        }

        void SyncLayout(Element& element)
        {
            const ComputedStyle& style = element.GetStyle();
            YGNodeRef node = element.GetNode();
            SyncFlex(node, style);
            SyncSize(node, style);
            SyncEdges(node, style);
            SyncMeasure(node, element);
            for (const auto& child : element.GetChildren())
            {
                SyncLayout(*child);
            }
        }

        void ReadLayout(Element& element, float parentX, float parentY)
        {
            YGNodeRef node = element.GetNode();
            float x = parentX + YGNodeLayoutGetLeft(node);
            float y = parentY + YGNodeLayoutGetTop(node);
            element.SetLayout(x, y, YGNodeLayoutGetWidth(node), YGNodeLayoutGetHeight(node));
            for (const auto& child : element.GetChildren())
            {
                ReadLayout(*child, x, y);
            }
        }
    }

    void Calculate(Element& root, float width, float height)
    {
        SyncLayout(root);
        YGNodeStyleSetWidth(root.GetNode(), width);
        YGNodeStyleSetHeight(root.GetNode(), height);
        YGNodeCalculateLayout(root.GetNode(), width, height, YGDirectionLTR);
        ReadLayout(root, 0.0f, 0.0f);
    }
}
