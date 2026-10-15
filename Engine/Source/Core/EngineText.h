#pragma once

#include "Core/Assert.h"

#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace BrightBox3D::Core
{
    enum class EngineText
    {
        PlayerCannotOpenProject,
        PlayerCannotReadManifest,
        PlayerModuleLoadFailed,
        PlayerStartupLevelFailed,
        SavedScreenshot,
        CssUnknownPseudoClass,
        CssMissingBrace,
        CorruptCookedFont,
        BindingNeedsRootAndProperty,
        BindingUnknownSource,
        BindingNotUIProperty,
        BindingReadOnly,
        BindingCannotResolve,
        DefaultFontMissing,
        DefaultFontFailed,
        FontNotFound,
        CssUnknownProperty,
        CssInvalidValue,
        UIStyleWarning,
        UIAssetNotFound,
        StyleSheetFailed,
        UINestingTooDeep,
        WidgetParseFailed,
        UnknownUIElement,
        UnknownUIAttribute,
        UIControllerNotLoaded,
        UIXmlError,
        UIEmptyScreen,
        UIScreenNotFound,
        UIBindingError,
        FreeTypeInitFailed,
        FontLoadFailed,
        GlyphPackingFailed,
        UIScreenLoadFailed,
        AssertionFailed,
        LoadLibraryFailed,
        DlopenFailed,
        CommandLineUnknownOption,
        CommandLineMissingValue,
        CommandLineInvalidNumber,
        CommandLineExtraArgument,
        CommandLineUsage,
        CommandLineNumberValue,
        CommandLinePathValue,
        CommandLineTextValue,
        LogLevelInfo,
        LogLevelWarning,
        LogLevelError,
        JsonParseError,
        CannotReadFile,
        UnknownType,
        TypeNotInstantiable,
        GameStarted,
        ClassNotLoadedFallback,
        NoDefaultPlayable,
        DefaultPlayableWrongType,
        GlfwError,
        PlatformInitFailed,
        WindowCreateFailed,
        GameModeMigrated,
        ModuleCopyFailed,
        GameModuleLoaded,
        ModuleRestored,
        ModuleRestoreFailed,
        GameModuleReloaded,
        ModuleMissing,
        ProjectExists,
        ProjectFoldersFailed,
        ProjectWriteFailed,
        ProjectInvalid,
        ManifestInvalid,
        VariantMigrationFailed,
        VariantMigrated,
        ShaderIncludeDepth,
        ShaderMalformedInclude,
        ShaderIncludeMissing,
        ShaderFileMissing,
        SpirvCrossError,
        MetalOnlyOnApple,
        ShaderLoadFailed,
        ShaderCompileFailed,
        PipelineFailed,
        CorruptCookedTexture,
        ModelHasNoMeshes,
        CorruptCookedModel,
        ConvexHullFallback,
        ConvexHullFailed,
        MeshColliderFailed,
        StreamingFailed,
        ReloadComponentMissing,
        ReloadObjectTypeMissing,
        ReloadTypeFallback,
        ReloadTypeRemoved,
        ObjectIdCollision,
        ParentCycle,
        ParentOtherLevel,
        ComponentAbstract,
        ComponentDropped,
        MissingComponentType,
        LinkedAssetMissing,
        VariantParentInvalid,
        MissingObjectType,
        TemplateNotPlaceable,
        CollectionLoadFailed,
        LevelAssetNotFound,
        LevelWriteFailed,
        CorruptCookedJson,
        DuplicateAssetGuid,
        NoImporter,
        CookedWriteFailed,
        AssetLoadFailed,
        MissingCookedData,
        CookedManifestMissing,
        AssetWriteFailed,
        TypeRegisteredTwice,
        MetalTextureFailed,
        MslCompileFailed,
        UnknownError,
        EntryPointMissing,
        PipelineCreateFailed,
        MetalUnavailable,
        DX11BufferFailed,
        DX11TextureFailed,
        D3DCompileFailed,
        InputLayoutFailed,
        DX11DeviceFailed,
        BindingNotPropertyOrComponent,
        BindingNoProperty,
        ClassNotLoaded,
        NotLevelObjectType,
        WorldTypeAsEngineObject,
        TypeAbstract,
        NotAComponent,
        ProcessStartFailed,
        TextFileNotObject,
        NewFolderName,
        NewLevelName,
        NewCollectionName,
        NewTemplateName,
        NewMaterialName,
        NewScreenName,
        NewWidgetName,
        NewStyleSheetName,
        Count
    };

    struct EngineTextInfo
    {
        const char* key;
        const char* english;
        int argumentCount;
    };

    const EngineTextInfo& GetTextInfo(EngineText text);
    std::optional<EngineText> FindEngineText(std::string_view key);

    class EngineTextTable
    {
    public:
        static EngineTextTable& Get();

        const std::string& Lookup(EngineText text) const { return m_texts[static_cast<size_t>(text)]; }
        bool SetOverride(EngineText text, std::string value);
        void Reset();

    private:
        EngineTextTable();

        std::array<std::string, static_cast<size_t>(EngineText::Count)> m_texts;
    };

    inline const char* Text(EngineText text)
    {
        return EngineTextTable::Get().Lookup(text).c_str();
    }

    template <typename... Args>
    std::string Text(EngineText text, const Args&... arguments)
    {
        BrightAssert(GetTextInfo(text).argumentCount == static_cast<int>(sizeof...(Args)), "EngineText used with the wrong number of arguments");
        return std::vformat(EngineTextTable::Get().Lookup(text), std::make_format_args(arguments...));
    }
}
