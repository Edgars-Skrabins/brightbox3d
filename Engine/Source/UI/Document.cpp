#include "UI/Document.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "Core/Utf8.h"
#include "Level/Level.h"
#include "Reflection/TypeRegistry.h"
#include "Rendering/GpuResourceCache.h"
#include "UI/CssProperties.h"
#include "UI/DrawList.h"
#include "UI/Font.h"
#include "UI/Markup.h"
#include "UI/SelectorMatching.h"
#include "UI/StyleSheet.h"
#include "UI/UIAssets.h"
#include "UI/WidgetController.h"

#include <pugixml.hpp>
#include <yoga/Yoga.h>

#include <algorithm>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr const char* DefaultStyleSheet = R"(
Screen { width: 100%; height: 100%; }
Label { color: #ffffff; }
Button { padding: 6px 14px; background-color: #3a3d42; border-radius: 4px; align-items: center; justify-content: center; text-align: center; }
Button:hover { background-color: #4a4e55; }
Button:active { background-color: #2a5c96; }
Button:disabled { opacity: 0.5; }
TextField { padding: 6px 8px; background-color: #1d1f22; border: 1px #44484f; border-radius: 3px; min-width: 140px; }
TextField:focus { border-color: #4a8be0; }
ProgressBar { height: 14px; min-width: 100px; background-color: #1d1f22; border-radius: 3px; }
Slider { height: 16px; min-width: 120px; background-color: #1d1f22; border-radius: 8px; }
Toggle { width: 18px; height: 18px; background-color: #1d1f22; border: 1px #5a5e65; border-radius: 3px; }
Toggle:checked { background-color: #2a5c96; }
Image { width: 64px; height: 64px; }
)";

        constexpr YGFlexDirection YogaFlexDirections[] = { YGFlexDirectionColumn, YGFlexDirectionRow, YGFlexDirectionColumnReverse, YGFlexDirectionRowReverse };
        constexpr YGJustify YogaJustify[] = { YGJustifyFlexStart, YGJustifyCenter, YGJustifyFlexEnd, YGJustifySpaceBetween, YGJustifySpaceAround, YGJustifySpaceEvenly };
        constexpr YGAlign YogaAlign[] = { YGAlignAuto, YGAlignFlexStart, YGAlignCenter, YGAlignFlexEnd, YGAlignStretch, YGAlignBaseline, YGAlignSpaceBetween, YGAlignSpaceAround };
        constexpr YGWrap YogaWrap[] = { YGWrapNoWrap, YGWrapWrap };
        constexpr YGPositionType YogaPositionType[] = { YGPositionTypeRelative, YGPositionTypeAbsolute };
        constexpr YGOverflow YogaOverflow[] = { YGOverflowVisible, YGOverflowHidden };

        static_assert(std::size(YogaFlexDirections) == static_cast<size_t>(FlexDirection::Count));
        static_assert(std::size(YogaJustify) == static_cast<size_t>(Justify::Count));
        static_assert(std::size(YogaAlign) == static_cast<size_t>(Align::Count));
        static_assert(std::size(YogaWrap) == static_cast<size_t>(FlexWrap::Count));
        static_assert(std::size(YogaPositionType) == static_cast<size_t>(PositionType::Count));
        static_assert(std::size(YogaOverflow) == static_cast<size_t>(Overflow::Count));

        template <typename Yoga, size_t Size, typename Value>
        Yoga ToYoga(const Yoga (&table)[Size], Value value)
        {
            return table[static_cast<size_t>(value)];
        }

        void LogStyleWarnings(const std::string& source, const std::vector<std::string>& warnings)
        {
            for (const std::string& warning : warnings)
            {
                Core::Log::Warning(Core::Text(Core::EngineText::UIStyleWarning, source, warning));
            }
        }

        const std::string& DisplayText(const Element& element)
        {
            if (element.GetElementType() == ElementType::TextField && element.GetText().empty() && !element.IsFocused())
            {
                return element.GetPlaceholder();
            }
            return element.GetText();
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
            std::vector<TextLine> lines = font->LayoutLines(DisplayText(*element), size, maxWidth);
            float measuredWidth = 0.0f;
            for (const TextLine& line : lines)
            {
                measuredWidth = std::max(measuredWidth, line.width);
            }
            float measuredHeight = font->LineHeight(size) * static_cast<float>(std::max<size_t>(lines.size(), 1));
            if (widthMode == YGMeasureModeExactly)
            {
                measuredWidth = width;
            }
            else if (widthMode == YGMeasureModeAtMost)
            {
                measuredWidth = std::min(measuredWidth, width);
            }
            if (heightMode == YGMeasureModeExactly)
            {
                measuredHeight = height;
            }
            else if (heightMode == YGMeasureModeAtMost)
            {
                measuredHeight = std::min(measuredHeight, height);
            }
            return { std::ceil(measuredWidth), std::ceil(measuredHeight) };
        }

        bool UsesTextMeasure(const Element& element)
        {
            ElementType type = element.GetElementType();
            return element.GetChildren().empty() && (type == ElementType::Label || type == ElementType::Button || type == ElementType::TextField);
        }
    }

    Element* WidgetController::Find(const std::string& name) const
    {
        return m_root ? m_root->FindByName(name) : nullptr;
    }

    Level::LevelObject* WidgetController::GetPlayer() const
    {
        return Reflection::Cast<Level::LevelObject>(GetBindingRoot(BindingRoot::Player));
    }

    Level::Level* WidgetController::GetLevel() const
    {
        return m_document ? m_document->GetRoots().level : nullptr;
    }

    Reflection::EngineObject* WidgetController::GetBindingRoot(BindingRoot root) const
    {
        return m_document ? m_document->GetRoots().Get(root) : nullptr;
    }

    Document::Document()
    {
        std::string error;
        m_styles.Parse(DefaultStyleSheet, error);
    }

    Document::~Document()
    {
        for (const auto& controller : m_controllers)
        {
            controller->OnDestroyed();
        }
        m_controllers.clear();
        m_root.reset();
    }

    Core::Guid Document::ResolveAsset(const std::string& reference, Assets::AssetType expectedType)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        Core::Guid guid = Core::Guid::Parse(reference);
        if (!guid.IsValid())
        {
            std::string path = reference;
            if (Core::StartsWith(path, "Assets/"))
            {
                path = path.substr(7);
            }
            if (const Assets::AssetRecord* record = database.FindRecordByPath(path))
            {
                guid = record->guid;
            }
        }
        if (guid.IsValid())
        {
            m_dependencies.insert(guid);
        }
        else
        {
            Core::Log::Warning(Core::Text(Core::EngineText::UIAssetNotFound, Assets::AssetTypeName(expectedType), reference));
        }
        return guid;
    }

    bool Document::LoadStyleSheet(const std::string& reference)
    {
        Core::Guid guid = ResolveAsset(reference, StyleSheetAsset::Type);
        auto asset = Assets::AssetDatabase::Get().Load<StyleSheetAsset>(guid);
        if (!asset)
        {
            return false;
        }
        StyleSheet sheet;
        std::string error;
        if (!sheet.Parse(asset->GetText(), error))
        {
            Core::Log::Error(Core::Text(Core::EngineText::StyleSheetFailed, reference, error));
            return false;
        }
        LogStyleWarnings(reference, sheet.GetWarnings());
        m_styles.Append(sheet);
        return true;
    }

    std::unique_ptr<Element> Document::BuildElement(const pugi::xml_node& node, int depth, std::string& error)
    {
        if (depth > 64)
        {
            error = Core::Text(Core::EngineText::UINestingTooDeep);
            return nullptr;
        }
        std::string name = node.name();
        std::optional<MarkupElement> markup = FindMarkupElement(name);
        if (markup == MarkupElement::Style)
        {
            if (const char* source = node.attribute(MarkupAttributeName(MarkupAttribute::Src)).value(); source && *source)
            {
                LoadStyleSheet(source);
            }
            else
            {
                StyleSheet sheet;
                std::string styleError;
                if (sheet.Parse(node.child_value(), styleError))
                {
                    LogStyleWarnings(name, sheet.GetWarnings());
                    m_styles.Append(sheet);
                }
            }
            return nullptr;
        }

        pugi::xml_document widgetDocument;
        pugi::xml_node source = node;
        if (markup == MarkupElement::Use)
        {
            Core::Guid guid = ResolveAsset(node.attribute(MarkupAttributeName(MarkupAttribute::Widget)).value(), WidgetAsset::Type);
            auto widget = Assets::AssetDatabase::Get().Load<WidgetAsset>(guid);
            if (!widget)
            {
                return nullptr;
            }
            pugi::xml_parse_result result = widgetDocument.load_string(widget->GetText().c_str());
            if (!result)
            {
                Core::Log::Error(Core::Text(Core::EngineText::WidgetParseFailed, widget->GetPath(), result.description()));
                return nullptr;
            }
            source = widgetDocument.document_element();
        }

        std::optional<MarkupElement> sourceMarkup = FindMarkupElement(source.name());
        if (!sourceMarkup)
        {
            Core::Log::Warning(Core::Text(Core::EngineText::UnknownUIElement, source.name()));
        }
        const MarkupElementInfo& info = GetMarkupElementInfo(sourceMarkup.value_or(MarkupElement::Panel));
        ElementType type = info.type;
        auto element = std::make_unique<Element>(type, sourceMarkup ? std::string(source.name()) : std::string(info.name));
        MarkupValues values;
        auto readAttributes = [&](const pugi::xml_node& from)
        {
            for (pugi::xml_attribute attribute : from.attributes())
            {
                std::optional<MarkupAttribute> key = FindMarkupAttribute(attribute.name());
                if (!key)
                {
                    Core::Log::Warning(Core::Text(Core::EngineText::UnknownUIAttribute, attribute.name(), from.name()));
                    continue;
                }
                values[static_cast<size_t>(*key)] = attribute.value();
            }
        };
        readAttributes(source);
        if (source != node)
        {
            readAttributes(node);
        }
        ApplyAttributes(*element, values, source);

        if (const char* controllerName = source.attribute(MarkupAttributeName(MarkupAttribute::Controller)).value(); controllerName && *controllerName)
        {
            const Reflection::TypeInfo* controllerType = Reflection::TypeRegistry::Find(controllerName);
            if (controllerType && controllerType->IsA(WidgetController::StaticType()) && !controllerType->IsAbstract())
            {
                std::unique_ptr<WidgetController> controller(static_cast<WidgetController*>(controllerType->CreateInstance()));
                controller->m_root = element.get();
                controller->m_document = this;
                element->SetController(controller.get());
                m_controllers.push_back(std::move(controller));
            }
            else
            {
                Core::Log::Warning(Core::Text(Core::EngineText::UIControllerNotLoaded, controllerName));
            }
        }

        for (pugi::xml_node child : source.children())
        {
            if (child.type() != pugi::node_element)
            {
                continue;
            }
            if (auto built = BuildElement(child, depth + 1, error))
            {
                element->AddChild(std::move(built));
            }
            if (!error.empty())
            {
                return nullptr;
            }
        }
        return element;
    }

    void Document::ApplyAttributes(Element& element, const MarkupValues& values, const pugi::xml_node& source)
    {
        auto value = [&](MarkupAttribute attribute) -> const std::optional<std::string>&
        {
            return values[static_cast<size_t>(attribute)];
        };
        auto bindOrApply = [&](MarkupAttribute attribute, const std::string& text, auto apply)
        {
            TextTemplate parsed = TextTemplate::Parse(text);
            if (parsed.HasBindings())
            {
                element.SetBinding(*BindingTargetFor(attribute), std::move(parsed));
                return;
            }
            apply(text);
        };
        auto bindOrApplyIfSet = [&](MarkupAttribute attribute, auto apply)
        {
            if (value(attribute))
            {
                bindOrApply(attribute, *value(attribute), apply);
            }
        };
        if (const std::optional<std::string>& name = value(MarkupAttribute::Name) ? value(MarkupAttribute::Name) : value(MarkupAttribute::Id))
        {
            element.SetName(*name);
        }
        if (value(MarkupAttribute::Class))
        {
            for (const std::string& part : Core::Split(*value(MarkupAttribute::Class), ' '))
            {
                element.AddClass(Core::Trim(part));
            }
        }
        if (value(MarkupAttribute::Style))
        {
            std::vector<std::string> warnings;
            element.GetInlineStyle() = ParseDeclarations(*value(MarkupAttribute::Style), warnings);
            LogStyleWarnings(element.GetTypeName(), warnings);
        }
        if (value(MarkupAttribute::Placeholder))
        {
            element.SetPlaceholder(*value(MarkupAttribute::Placeholder));
        }
        std::string text = value(MarkupAttribute::Text).value_or(Core::Trim(source.child_value()));
        if (!text.empty())
        {
            bindOrApply(MarkupAttribute::Text, text, [&](const std::string& content) { element.SetText(content); });
        }
        if (value(MarkupAttribute::Min) || value(MarkupAttribute::Max))
        {
            float minimum = value(MarkupAttribute::Min) ? ParseMarkupNumber(*value(MarkupAttribute::Min)) : 0.0f;
            float maximum = 1.0f;
            bindOrApplyIfSet(MarkupAttribute::Max, [&](const std::string& number) { maximum = ParseMarkupNumber(number); });
            element.SetRange(minimum, maximum);
        }
        bindOrApplyIfSet(MarkupAttribute::Value, [&](const std::string& number) { element.SetValue(ParseMarkupNumber(number)); });
        bindOrApplyIfSet(MarkupAttribute::Checked, [&](const std::string& flag) { element.SetBool(ParseMarkupBool(flag)); });
        bindOrApplyIfSet(MarkupAttribute::Disabled, [&](const std::string& flag) { element.SetDisabled(ParseMarkupBool(flag)); });
        bindOrApplyIfSet(MarkupAttribute::Visible, [&](const std::string& flag) { element.SetMarkupHidden(flag == MarkupFalse); });
        bindOrApplyIfSet(MarkupAttribute::OnClick, [](const std::string&) {});
        if (element.GetElementType() == ElementType::Image && value(MarkupAttribute::Src))
        {
            element.SetImage(ResolveAsset(*value(MarkupAttribute::Src), Assets::AssetType::Texture));
        }
    }

    bool Document::LoadFromText(const std::string& markup, std::string& error)
    {
        pugi::xml_document document;
        pugi::xml_parse_result result = document.load_string(markup.c_str());
        if (!result)
        {
            error = Core::Text(Core::EngineText::UIXmlError, result.description(), result.offset);
            return false;
        }
        m_root = BuildElement(document.document_element(), 0, error);
        if (!m_root)
        {
            if (error.empty())
            {
                error = Core::Text(Core::EngineText::UIEmptyScreen);
            }
            return false;
        }
        return true;
    }

    bool Document::LoadScreen(const Core::Guid& screen, std::string& error)
    {
        m_dependencies.insert(screen);
        auto asset = Assets::AssetDatabase::Get().Load<ScreenAsset>(screen);
        if (!asset)
        {
            error = Core::Text(Core::EngineText::UIScreenNotFound);
            return false;
        }
        return LoadFromText(asset->GetText(), error);
    }

    void Document::ComputeStyles(Element& element, const ComputedStyle* parent)
    {
        ComputedStyle style;
        if (parent)
        {
            style.color = parent->color;
            style.fontSize = parent->fontSize;
            style.fontFamily = parent->fontFamily;
            style.textAlign = parent->textAlign;
        }
        std::vector<std::pair<const StyleRule*, int>> matches;
        for (const StyleRule& rule : m_styles.GetRules())
        {
            int best = -1;
            for (const Selector& selector : rule.selectors)
            {
                if (Matches(selector, element))
                {
                    best = std::max(best, selector.specificity);
                }
            }
            if (best >= 0)
            {
                matches.emplace_back(&rule, best);
            }
        }
        std::stable_sort(matches.begin(), matches.end(), [](const auto& a, const auto& b)
        {
            return a.second != b.second ? a.second < b.second : a.first->order < b.first->order;
        });
        for (const auto& [rule, specificity] : matches)
        {
            for (const CssDeclaration& declaration : rule->declarations)
            {
                declaration.apply(style);
            }
        }
        for (const CssDeclaration& declaration : element.GetInlineStyle())
        {
            declaration.apply(style);
        }
        if (element.IsMarkupHidden() || element.IsBindingHidden())
        {
            style.display = Display::None;
        }
        element.GetStyle() = style;
        for (const auto& child : element.GetChildren())
        {
            ComputeStyles(*child, &element.GetStyle());
        }
    }

    void Document::SyncLayout(Element& element)
    {
        const ComputedStyle& style = element.GetStyle();
        YGNodeRef node = element.GetNode();
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

        auto setDimension = [&](const Length& length, auto setPoints, auto setPercent, auto setAuto, auto setUndefined)
        {
            switch (length.unit)
            {
            case LengthUnit::Pixels:
                setPoints(node, length.value);
                break;
            case LengthUnit::Percent:
                setPercent(node, length.value);
                break;
            case LengthUnit::Auto:
                setAuto(node);
                break;
            default:
                setUndefined(node);
                break;
            }
        };
        setDimension(style.width, YGNodeStyleSetWidth, YGNodeStyleSetWidthPercent, YGNodeStyleSetWidthAuto, YGNodeStyleSetWidthAuto);
        setDimension(style.height, YGNodeStyleSetHeight, YGNodeStyleSetHeightPercent, YGNodeStyleSetHeightAuto, YGNodeStyleSetHeightAuto);
        setDimension(style.flexBasis, YGNodeStyleSetFlexBasis, YGNodeStyleSetFlexBasisPercent, YGNodeStyleSetFlexBasisAuto, YGNodeStyleSetFlexBasisAuto);
        auto setLimit = [&](const Length& length, auto setPoints, auto setPercent)
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
        };
        setLimit(style.minWidth, YGNodeStyleSetMinWidth, YGNodeStyleSetMinWidthPercent);
        setLimit(style.minHeight, YGNodeStyleSetMinHeight, YGNodeStyleSetMinHeightPercent);
        setLimit(style.maxWidth, YGNodeStyleSetMaxWidth, YGNodeStyleSetMaxWidthPercent);
        setLimit(style.maxHeight, YGNodeStyleSetMaxHeight, YGNodeStyleSetMaxHeightPercent);

        const YGEdge edges[4] = { YGEdgeTop, YGEdgeRight, YGEdgeBottom, YGEdgeLeft };
        for (int i = 0; i < 4; ++i)
        {
            const Length& margin = style.margin[i];
            if (margin.unit == LengthUnit::Auto)
            {
                YGNodeStyleSetMarginAuto(node, edges[i]);
            }
            else if (margin.unit == LengthUnit::Percent)
            {
                YGNodeStyleSetMarginPercent(node, edges[i], margin.value);
            }
            else
            {
                YGNodeStyleSetMargin(node, edges[i], margin.unit == LengthUnit::Pixels ? margin.value : 0.0f);
            }
            const Length& padding = style.padding[i];
            if (padding.unit == LengthUnit::Percent)
            {
                YGNodeStyleSetPaddingPercent(node, edges[i], padding.value);
            }
            else
            {
                YGNodeStyleSetPadding(node, edges[i], padding.unit == LengthUnit::Pixels ? padding.value : 0.0f);
            }
            const Length& inset = style.inset[i];
            if (inset.unit == LengthUnit::Percent)
            {
                YGNodeStyleSetPositionPercent(node, edges[i], inset.value);
            }
            else
            {
                YGNodeStyleSetPosition(node, edges[i], inset.unit == LengthUnit::Pixels ? inset.value : YGUndefined);
            }
        }
        YGNodeStyleSetBorder(node, YGEdgeAll, style.borderWidth);
        YGNodeStyleSetGap(node, YGGutterAll, style.gap);

        if (UsesTextMeasure(element))
        {
            if (!YGNodeHasMeasureFunc(node))
            {
                YGNodeSetMeasureFunc(node, &MeasureText);
            }
            YGNodeMarkDirty(node);
        }
        for (const auto& child : element.GetChildren())
        {
            SyncLayout(*child);
        }
    }

    void Document::ReadLayout(Element& element, float parentX, float parentY)
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

    void Document::ReportBindingError(const std::string& error)
    {
        if (!error.empty() && m_reportedErrors.insert(error).second)
        {
            Core::Log::Error(Core::Text(Core::EngineText::UIBindingError, error));
        }
    }

    void Document::UpdateBindings(Element& element)
    {
        std::string error;
        const TextTemplate* text = element.GetBinding(BindingTarget::Text);
        if (text && !(element.GetElementType() == ElementType::TextField && element.IsFocused()))
        {
            if (element.GetElementType() == ElementType::TextField && text->IsSingleBinding())
            {
                BoundProperty bound = ResolveBinding(text->segments[0].text, m_roots, BindingAccess::Write, error);
                if (bound)
                {
                    element.SetText(bound.Format(text->segments[0].decimals));
                }
            }
            else
            {
                element.SetText(text->Evaluate(m_roots, error));
            }
            ReportBindingError(error);
            error.clear();
        }
        auto readNumber = [&](BindingTarget target, auto apply, BindingAccess access)
        {
            const TextTemplate* binding = element.GetBinding(target);
            if (!binding || !binding->IsSingleBinding())
            {
                return;
            }
            BoundProperty bound = ResolveBinding(binding->segments[0].text, m_roots, access, error);
            ReportBindingError(error);
            error.clear();
            float number = 0.0f;
            if (bound && bound.ReadNumber(number))
            {
                apply(number);
            }
        };
        if (element.GetElementType() == ElementType::ProgressBar)
        {
            readNumber(BindingTarget::Value, [&](float number) { element.SetValue(number); }, BindingAccess::Read);
            readNumber(BindingTarget::Max, [&](float number) { element.SetRange(element.GetMinimum(), number); }, BindingAccess::Read);
        }
        else if (element.GetElementType() == ElementType::Slider && !element.IsPressed())
        {
            readNumber(BindingTarget::Value, [&](float number) { element.SetValue(number); }, BindingAccess::Write);
        }
        if (element.GetElementType() == ElementType::Toggle)
        {
            readNumber(BindingTarget::Checked, [&](float number) { element.SetBool(number != 0.0f); }, BindingAccess::Write);
        }
        if (element.GetBinding(BindingTarget::Visible))
        {
            bool visible = true;
            readNumber(BindingTarget::Visible, [&](float number) { visible = number != 0.0f; }, BindingAccess::Read);
            element.SetBindingHidden(!visible);
            if (!visible)
            {
                element.GetStyle().display = Display::None;
            }
        }
        readNumber(BindingTarget::Disabled, [&](float number) { element.SetDisabled(number != 0.0f); }, BindingAccess::Read);
        for (const auto& child : element.GetChildren())
        {
            UpdateBindings(*child);
        }
    }

    void Document::WriteBinding(Element& element, BindingTarget target)
    {
        const TextTemplate* binding = element.GetBinding(target);
        if (!binding || !binding->IsSingleBinding())
        {
            return;
        }
        std::string error;
        BoundProperty bound = ResolveBinding(binding->segments[0].text, m_roots, BindingAccess::Write, error);
        ReportBindingError(error);
        if (!bound)
        {
            return;
        }
        switch (target)
        {
        case BindingTarget::OnClick:
            bound.Trigger();
            break;
        case BindingTarget::Checked:
            bound.WriteBool(element.GetBool());
            break;
        case BindingTarget::Value:
            bound.WriteNumber(element.GetValue());
            break;
        case BindingTarget::Text:
            bound.WriteString(element.GetText());
            break;
        default:
            break;
        }
    }

    void Document::NotifyValueChanged(Element& element)
    {
        if (WidgetController* controller = element.FindController())
        {
            controller->OnValueChanged(element);
        }
    }

    Element* Document::HitTest(Element& element, float x, float y)
    {
        if (element.GetStyle().IsHidden())
        {
            return nullptr;
        }
        bool inside = element.Contains(x, y);
        if (element.GetStyle().overflow == Overflow::Hidden && !inside)
        {
            return nullptr;
        }
        for (auto it = element.GetChildren().rbegin(); it != element.GetChildren().rend(); ++it)
        {
            if (Element* hit = HitTest(**it, x, y))
            {
                return hit;
            }
        }
        return inside && element.IsInteractive() && !element.IsDisabled() ? &element : nullptr;
    }

    void Document::HandleInput(const UIInput& input)
    {
        Element* hovered = input.pointerEnabled ? HitTest(*m_root, input.mouse.x, input.mouse.y) : nullptr;
        if (m_hovered && m_hovered != hovered)
        {
            m_hovered->SetHovered(false);
        }
        m_hovered = hovered;
        if (m_hovered)
        {
            m_hovered->SetHovered(true);
        }

        if (input.mousePressed)
        {
            if (m_focused && m_focused != hovered)
            {
                m_focused->SetFocused(false);
                WriteBinding(*m_focused, BindingTarget::Text);
                NotifyValueChanged(*m_focused);
                m_focused = nullptr;
            }
            if (hovered)
            {
                m_pressed = hovered;
                m_pressed->SetPressed(true);
                if (hovered->GetElementType() == ElementType::TextField)
                {
                    m_focused = hovered;
                    m_focused->SetFocused(true);
                }
            }
        }

        if (m_pressed && m_pressed->GetElementType() == ElementType::Slider && input.mouseDown)
        {
            float width = std::max(m_pressed->GetWidth(), 1.0f);
            float t = Math::Clamp01((input.mouse.x - m_pressed->GetX()) / width);
            float value = Math::Lerp(m_pressed->GetMinimum(), m_pressed->GetMaximum(), t);
            if (value != m_pressed->GetValue())
            {
                m_pressed->SetValue(value);
                WriteBinding(*m_pressed, BindingTarget::Value);
                NotifyValueChanged(*m_pressed);
            }
        }

        if (input.mouseReleased && m_pressed)
        {
            Element* released = m_pressed;
            released->SetPressed(false);
            m_pressed = nullptr;
            if (released == hovered)
            {
                if (released->GetElementType() == ElementType::Button)
                {
                    WriteBinding(*released, BindingTarget::OnClick);
                    if (WidgetController* controller = released->FindController())
                    {
                        controller->OnClicked(*released);
                    }
                }
                else if (released->GetElementType() == ElementType::Toggle)
                {
                    released->SetBool(!released->GetBool());
                    WriteBinding(*released, BindingTarget::Checked);
                    NotifyValueChanged(*released);
                }
            }
        }

        if (m_focused)
        {
            std::string text = m_focused->GetText();
            bool changed = false;
            for (char32_t codepoint : input.text)
            {
                if (codepoint >= 0x20)
                {
                    text += Core::Utf8::Encode(codepoint);
                    changed = true;
                }
            }
            if (input.backspace && !text.empty())
            {
                Core::Utf8::PopCodepoint(text);
                changed = true;
            }
            if (changed)
            {
                m_focused->SetText(text);
            }
            if (input.enter || input.escape)
            {
                m_focused->SetFocused(false);
                WriteBinding(*m_focused, BindingTarget::Text);
                NotifyValueChanged(*m_focused);
                m_focused = nullptr;
            }
        }
    }

    void Document::Update(const BindingRoots& roots, const UIInput& input, float width, float height, float deltaTime)
    {
        if (!m_root)
        {
            return;
        }
        m_roots = roots;
        if (!m_created)
        {
            m_created = true;
            for (const auto& controller : m_controllers)
            {
                controller->OnCreated();
            }
        }
        UpdateBindings(*m_root);
        ComputeStyles(*m_root, nullptr);
        SyncLayout(*m_root);
        YGNodeStyleSetWidth(m_root->GetNode(), width);
        YGNodeStyleSetHeight(m_root->GetNode(), height);
        YGNodeCalculateLayout(m_root->GetNode(), width, height, YGDirectionLTR);
        ReadLayout(*m_root, 0.0f, 0.0f);
        HandleInput(input);
        for (const auto& controller : m_controllers)
        {
            controller->OnUpdate(deltaTime);
        }
    }

    void Document::DrawElement(const Element& element, DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources, float parentOpacity) const
    {
        const ComputedStyle& style = element.GetStyle();
        if (style.IsHidden())
        {
            return;
        }
        float opacity = parentOpacity * style.opacity;
        auto faded = [&](Math::Color color)
        {
            color.a *= opacity;
            return color;
        };
        float x = element.GetX();
        float y = element.GetY();
        float width = element.GetWidth();
        float height = element.GetHeight();
        drawList.AddRect(x, y, width, height, faded(style.backgroundColor), style.borderRadius);

        float paddingLeft = YGNodeLayoutGetPadding(element.GetNode(), YGEdgeLeft) + style.borderWidth;
        float paddingRight = YGNodeLayoutGetPadding(element.GetNode(), YGEdgeRight) + style.borderWidth;
        float paddingTop = YGNodeLayoutGetPadding(element.GetNode(), YGEdgeTop) + style.borderWidth;
        float innerWidth = std::max(0.0f, width - paddingLeft - paddingRight);

        switch (element.GetElementType())
        {
        case ElementType::ProgressBar:
        {
            float range = element.GetMaximum() - element.GetMinimum();
            float t = range != 0.0f ? Math::Clamp01((element.GetValue() - element.GetMinimum()) / range) : 0.0f;
            drawList.AddRect(x, y, width * t, height, faded(style.fillColor), style.borderRadius);
            break;
        }
        case ElementType::Slider:
        {
            float range = element.GetMaximum() - element.GetMinimum();
            float t = range != 0.0f ? Math::Clamp01((element.GetValue() - element.GetMinimum()) / range) : 0.0f;
            drawList.AddRect(x, y, width * t, height, faded(style.fillColor), style.borderRadius);
            float knob = height + 4.0f;
            drawList.AddRect(x + width * t - knob * 0.5f, y + height * 0.5f - knob * 0.5f, knob, knob, faded(style.color), knob * 0.5f);
            break;
        }
        case ElementType::Toggle:
            if (element.GetBool())
            {
                float inset = std::min(width, height) * 0.25f;
                drawList.AddRect(x + inset, y + inset, width - inset * 2.0f, height - inset * 2.0f, faded(style.color), style.borderRadius * 0.5f);
            }
            break;
        case ElementType::Image:
            if (Rendering::GpuTexture* texture = resources.GetTexture(element.GetImage()))
            {
                drawList.AddImage(x, y, width, height, texture->texture.get(), texture->texture->GetFormat() == RHI::Format::RGBA8Srgb, faded(style.color), style.borderRadius);
            }
            break;
        default:
            break;
        }

        const std::string& text = DisplayText(element);
        if (!text.empty() && (element.GetElementType() == ElementType::Label || element.GetElementType() == ElementType::Button || element.GetElementType() == ElementType::TextField))
        {
            Font* font = FontLibrary::Get().Get(style.fontFamily);
            RHI::Texture* atlas = font ? font->GetTexture(device) : nullptr;
            if (font && atlas)
            {
                const FontData& data = font->GetData();
                float size = style.fontSize;
                std::vector<TextLine> lines = font->LayoutLines(text, size, innerWidth + 0.5f);
                float lineHeight = font->LineHeight(size);
                float contentHeight = lineHeight * static_cast<float>(lines.size());
                float top = element.GetElementType() == ElementType::Label ? y + paddingTop : y + (height - contentHeight) * 0.5f;
                Math::Color color = faded(style.color);
                if (element.GetElementType() == ElementType::TextField && element.GetText().empty())
                {
                    color.a *= 0.45f;
                }
                float unitX = data.pixelRange / static_cast<float>(data.atlasWidth);
                float unitY = data.pixelRange / static_cast<float>(data.atlasHeight);
                float caretX = x + paddingLeft;
                float caretY = top;
                for (const TextLine& line : lines)
                {
                    float penX = x + paddingLeft;
                    if (style.textAlign == TextAlign::Center)
                    {
                        penX += (innerWidth - line.width) * 0.5f;
                    }
                    else if (style.textAlign == TextAlign::Right)
                    {
                        penX += innerWidth - line.width;
                    }
                    float baseline = top + data.ascender * size;
                    uint32_t previous = 0;
                    for (uint32_t codepoint : Core::Utf8::Decode(line.text))
                    {
                        const FontGlyph* glyph = data.FindGlyph(codepoint);
                        if (!glyph)
                        {
                            glyph = data.FindGlyph('?');
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
                            drawList.AddGlyph(penX + glyph->planeLeft * size, baseline - glyph->planeTop * size, penX + glyph->planeRight * size, baseline - glyph->planeBottom * size, glyph->atlasLeft, glyph->atlasTop, glyph->atlasRight, glyph->atlasBottom, atlas, color, unitX, unitY);
                        }
                        penX += glyph->advance * size;
                        previous = codepoint;
                    }
                    caretX = penX;
                    caretY = top;
                    top += lineHeight;
                }
                if (element.GetElementType() == ElementType::TextField && element.IsFocused())
                {
                    if (element.GetText().empty())
                    {
                        caretX = x + paddingLeft;
                    }
                    drawList.AddRect(caretX + 1.0f, caretY, 1.5f, lineHeight, faded(style.color), 0.0f);
                }
            }
        }
        else if (element.GetElementType() == ElementType::TextField && element.IsFocused())
        {
            Font* font = FontLibrary::Get().Get(style.fontFamily);
            float lineHeight = font ? font->LineHeight(style.fontSize) : style.fontSize;
            drawList.AddRect(x + paddingLeft + 1.0f, y + (height - lineHeight) * 0.5f, 1.5f, lineHeight, faded(style.color), 0.0f);
        }

        drawList.AddBorder(x, y, width, height, faded(style.borderColor), style.borderRadius, style.borderWidth);

        bool clip = style.overflow == Overflow::Hidden;
        if (clip)
        {
            drawList.PushClip(x, y, width, height);
        }
        for (const auto& child : element.GetChildren())
        {
            DrawElement(*child, drawList, device, resources, opacity);
        }
        if (clip)
        {
            drawList.PopClip();
        }
    }

    void Document::Draw(DrawList& drawList, RHI::Device& device, Rendering::GpuResourceCache& resources) const
    {
        if (m_root)
        {
            DrawElement(*m_root, drawList, device, resources, 1.0f);
        }
    }
}
