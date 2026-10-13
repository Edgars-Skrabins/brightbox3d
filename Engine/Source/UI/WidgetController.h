#pragma once

#include "Reflection/Object.h"

#include <string>

namespace BrightBox3D::Level
{
    class Level;
    class LevelObjectInstance;
}

namespace BrightBox3D::UI
{
    class Document;
    class Element;

    BrightClass(Abstract)
    class WidgetController : public Reflection::Object
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
        Level::LevelObjectInstance* GetPlayer() const;
        Level::Level* GetLevel() const;
        Reflection::Object* GetGameMode() const;

    private:
        friend class Document;

        Element* m_root = nullptr;
        Document* m_document = nullptr;
    };
}
