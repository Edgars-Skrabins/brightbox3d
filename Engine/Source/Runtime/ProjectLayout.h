#pragma once

#include "Assets/AssetDatabase.h"

namespace BrightBox3D::Runtime::ProjectLayout
{
    inline constexpr const char* ProjectFileName = "Project.bbproject";
    inline constexpr const char* AssetsFolder = Assets::AssetDatabase::AssetsFolder;
    inline constexpr const char* CacheFolder = Assets::AssetDatabase::CacheFolder;
    inline constexpr const char* SourceFolder = "Source";
    inline constexpr const char* BinariesFolder = "Binaries";
    inline constexpr const char* StagingFolder = "Staging";
    inline constexpr const char* BuildFolder = "Build";
    inline constexpr const char* CompileCacheFolder = "Build";
    inline constexpr const char* HotReloadCacheFolder = "HotReload";
    inline constexpr const char* GitIgnoreFileName = ".gitignore";
    inline constexpr const char* CMakeListsFileName = "CMakeLists.txt";
}
