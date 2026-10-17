#include "UI/DocumentBuilder.h"

#include "Assets/AssetDatabase.h"
#include "Core/EngineText.h"
#include "Core/Log.h"
#include "Core/StringUtils.h"
#include "Reflection/TypeRegistry.h"
#include "UI/CssProperties.h"
#include "UI/StyleSheet.h"
#include "UI/UIAssets.h"
#include "UI/WidgetController.h"

#include <pugixml.hpp>

namespace BrightBox3D::UI
{
    namespace
    {
        constexpr int MaxNestingDepth = 64;
        constexpr float DefaultRangeMinimum = 0.0f;
        constexpr float DefaultRangeMaximum = 1.0f;

        void LogStyleWarnings(const std::string& source, const std::vector<std::string>& warnings)
        {
            for (const std::string& warning : warnings)
            {
                Core::Log::Warning(Core::Text(Core::EngineText::UIStyleWarning, source, warning));
            }
        }
    }

    DocumentBuilder::DocumentBuilder(Document& document, StyleSheet& styles, std::vector<std::unique_ptr<WidgetController>>& controllers, std::unordered_set<Core::Guid>& dependencies)
        : m_document(document), m_styles(styles), m_controllers(controllers), m_dependencies(dependencies)
    {
    }

    Core::Result<std::unique_ptr<Element>> DocumentBuilder::Build(const pugi::xml_node& node)
    {
        return BuildElement(node, 0);
    }

    Core::Guid DocumentBuilder::ResolveAsset(const std::string& reference, Assets::AssetType expectedType)
    {
        Core::Guid guid = Assets::AssetDatabase::Get().ResolveReference(reference);
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

    bool DocumentBuilder::LoadStyleSheet(const std::string& reference)
    {
        Core::Guid guid = ResolveAsset(reference, StyleSheetAsset::Type);
        auto asset = Assets::AssetDatabase::Get().Load<StyleSheetAsset>(guid);
        if (!asset)
        {
            return false;
        }
        StyleSheet sheet;
        if (Core::Result<void> parsed = sheet.Parse(asset->GetText()); !parsed)
        {
            Core::Log::Error(Core::Text(Core::EngineText::StyleSheetFailed, reference, parsed.GetError().GetText()));
            return false;
        }
        LogStyleWarnings(reference, sheet.GetWarnings());
        m_styles.Append(sheet);
        return true;
    }

    void DocumentBuilder::LoadStyleElement(const pugi::xml_node& node)
    {
        if (const char* source = node.attribute(MarkupAttributeName(MarkupAttribute::Src)).value(); source && *source)
        {
            LoadStyleSheet(source);
            return;
        }
        StyleSheet sheet;
        if (Core::Result<void> parsed = sheet.Parse(node.child_value()); !parsed)
        {
            Core::Log::Error(Core::Text(Core::EngineText::StyleSheetFailed, node.name(), parsed.GetError().GetText()));
            return;
        }
        LogStyleWarnings(node.name(), sheet.GetWarnings());
        m_styles.Append(sheet);
    }

    Core::Result<std::unique_ptr<Element>> DocumentBuilder::BuildElement(const pugi::xml_node& node, int depth)
    {
        if (depth > MaxNestingDepth)
        {
            return Core::Error(Core::EngineText::UINestingTooDeep);
        }
        std::optional<MarkupElement> markup = FindMarkupElement(node.name());
        if (markup == MarkupElement::Style)
        {
            LoadStyleElement(node);
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
        auto element = std::make_unique<Element>(info.type, sourceMarkup ? std::string(source.name()) : std::string(info.name));
        MarkupValues values;
        ReadAttributes(source, values);
        if (source != node)
        {
            ReadAttributes(node, values);
        }
        ApplyAttributes(*element, values, source);

        if (const char* controllerName = source.attribute(MarkupAttributeName(MarkupAttribute::Controller)).value(); controllerName && *controllerName)
        {
            AttachController(*element, controllerName);
        }

        for (pugi::xml_node child : source.children())
        {
            if (child.type() != pugi::node_element)
            {
                continue;
            }
            Core::Result<std::unique_ptr<Element>> built = BuildElement(child, depth + 1);
            if (!built)
            {
                return built;
            }
            if (*built)
            {
                element->AddChild(std::move(*built));
            }
        }
        return element;
    }

    void DocumentBuilder::ReadAttributes(const pugi::xml_node& from, MarkupValues& values) const
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
    }

    void DocumentBuilder::ApplyAttributes(Element& element, const MarkupValues& values, const pugi::xml_node& source)
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
            float minimum = value(MarkupAttribute::Min) ? ParseMarkupNumber(*value(MarkupAttribute::Min)) : DefaultRangeMinimum;
            float maximum = DefaultRangeMaximum;
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

    void DocumentBuilder::AttachController(Element& element, const char* controllerName)
    {
        const Reflection::TypeInfo* controllerType = Reflection::TypeRegistry::Find(controllerName);
        if (!controllerType || !controllerType->IsA(WidgetController::StaticType()) || controllerType->IsAbstract())
        {
            Core::Log::Warning(Core::Text(Core::EngineText::UIControllerNotLoaded, controllerName));
            return;
        }
        std::unique_ptr<WidgetController> controller(static_cast<WidgetController*>(controllerType->CreateInstance()));
        controller->m_root = &element;
        controller->m_document = &m_document;
        element.SetController(controller.get());
        m_controllers.push_back(std::move(controller));
    }
}
