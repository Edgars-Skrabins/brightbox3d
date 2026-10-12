#pragma once

#include "Assets/Asset.h"
#include "Serialization/Json.h"

namespace BrightBox3D::Assets
{
    BrightClass()
    class JsonAsset : public Asset
    {
        BrightBody()
    public:
        Serialization::JsonDocument& GetDocument() { return m_document; }
        const Serialization::JsonDocument& GetDocument() const { return m_document; }

    private:
        Serialization::JsonDocument m_document;
    };
}
