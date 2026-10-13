#pragma once

#include "Assets/AssetType.h"
#include "Core/Guid.h"
#include "Reflection/EngineObject.h"

#include <string>

namespace BrightBox3D::Assets
{
    BrightClass(.abstract = true)
    class Asset : public Reflection::EngineObject
    {
        BrightBody()
    public:
        const Core::Guid& GetGuid() const { return m_guid; }
        const std::string& GetName() const { return m_name; }
        const std::string& GetPath() const { return m_path; }
        AssetType GetAssetType() const { return m_assetType; }

        void SetIdentity(const Core::Guid& guid, const std::string& name, const std::string& path, AssetType type)
        {
            m_guid = guid;
            m_name = name;
            m_path = path;
            m_assetType = type;
        }

    private:
        Core::Guid m_guid;
        std::string m_name;
        std::string m_path;
        AssetType m_assetType = AssetType::Unknown;
    };
}
