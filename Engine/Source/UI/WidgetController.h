#pragma once

#include "Reflection/EngineObject.h"
#include "UI/Binding.h"

#include <string>

namespace BrightBox3D::Level
{
    class Level;
    class LevelObject;
}

namespace BrightBox3D::UI
{
    class Document;
    class Element;

    BrightClass(.abstract = true)
    class WidgetController : public Reflection::EngineObject
    {
        BrightBody()
    public:
        virtual void OnCreated() {}
        virtual void OnUpdate(float deltaTime) {}
        virtual void OnClicked(Element& element) {}
        virtual void OnValueChanged(Element& element) {}
        virtual void OnDestroyed() {}

        Element* GetRoot() const { return m_root; }
        Element* Find(const std::string& name) const;
        Document* GetDocument() const { return m_document; }
        Level::LevelObject* GetPlayer() const;
        Level::Level* GetLevel() const override;
        Reflection::EngineObject* GetBindingRoot(BindingRoot root) const;

    private:
        friend class DocumentBuilder;

        Element* m_root = nullptr;
        Document* m_document = nullptr;
    };
}
