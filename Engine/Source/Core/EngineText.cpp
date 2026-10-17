#include "Core/EngineText.h"

#include "Core/StringUtils.h"

#include <iterator>

namespace BrightBox3D::Core
{
    namespace
    {
        const EngineTextInfo TextInfos[] = {
            { "PlayerCannotOpenProject", "Cannot open project: {}", 1 },
            { "PlayerCannotReadManifest", "Cannot read {}: {}", 2 },
            { "PlayerModuleLoadFailed", "Game module failed to load: {}", 1 },
            { "PlayerStartupLevelFailed", "Startup level could not be loaded: {}", 1 },
            { "SavedScreenshot", "Saved screenshot '{}'", 1 },
            { "CssUnknownPseudoClass", "unknown CSS pseudo-class ':{}'", 1 },
            { "CssMissingBrace", "missing '}'", 0 },
            { "CorruptCookedFont", "corrupt cooked font", 0 },
            { "BindingNeedsRootAndProperty", "binding '{}' must start with one of {} and name a property", 2 },
            { "BindingUnknownSource", "unknown binding source '{}' (use one of {})", 2 },
            { "BindingNotUIProperty", "'{}' is not a BrightUIProperty", 1 },
            { "BindingReadOnly", "'{}' is a Read BrightUIProperty and cannot be written by the UI", 1 },
            { "BindingCannotResolve", "cannot resolve '{}' in '{}'", 2 },
            { "DefaultFontMissing", "Default UI font is missing: {}", 1 },
            { "DefaultFontFailed", "Default UI font generation failed: {}", 1 },
            { "FontNotFound", "UI font '{}' was not found; using the default font", 1 },
            { "CssUnknownProperty", "unknown CSS property '{}'", 1 },
            { "CssInvalidValue", "invalid value '{}' for CSS property '{}'", 2 },
            { "UIStyleWarning", "UI style '{}': {}", 2 },
            { "UIAssetNotFound", "UI {} '{}' was not found", 2 },
            { "StyleSheetFailed", "Stylesheet '{}': {}", 2 },
            { "UINestingTooDeep", "UI nesting is too deep (recursive widget?)", 0 },
            { "WidgetParseFailed", "Widget '{}': {}", 2 },
            { "UnknownUIElement", "Unknown UI element <{}>; treating it as a Panel", 1 },
            { "UnknownUIAttribute", "Unknown UI attribute '{}' on <{}>", 2 },
            { "UIControllerNotLoaded", "UI controller class '{}' is not loaded", 1 },
            { "UIXmlError", "XML error: {} at offset {}", 2 },
            { "UIEmptyScreen", "empty screen", 0 },
            { "UIScreenNotFound", "screen asset not found", 0 },
            { "UIBindingError", "UI binding: {}", 1 },
            { "FreeTypeInitFailed", "FreeType initialization failed", 0 },
            { "FontLoadFailed", "cannot load font '{}'", 1 },
            { "GlyphPackingFailed", "glyph packing failed", 0 },
            { "UIScreenLoadFailed", "UI Screen failed to load: {}", 1 },
            { "AssertionFailed", "Assertion failed: {} {}", 2 },
            { "LoadLibraryFailed", "LoadLibrary failed with error {}", 1 },
            { "DlopenFailed", "dlopen failed", 0 },
            { "CommandLineUnknownOption", "unknown option '{}'", 1 },
            { "CommandLineMissingValue", "option '{}' needs a value", 1 },
            { "CommandLineInvalidNumber", "option '{}' expects a number, got '{}'", 2 },
            { "CommandLineExtraArgument", "unexpected argument '{}'", 1 },
            { "CommandLineUsage", "Usage: {}{}", 2 },
            { "CommandLineNumberValue", "number", 0 },
            { "CommandLinePathValue", "path", 0 },
            { "CommandLineTextValue", "text", 0 },
            { "LogLevelInfo", "Info", 0 },
            { "LogLevelWarning", "Warning", 0 },
            { "LogLevelError", "Error", 0 },
            { "JsonParseError", "{} (line {})", 2 },
            { "CannotReadFile", "cannot read file", 0 },
            { "UnknownType", "Unknown type '{}'", 1 },
            { "TypeNotInstantiable", "Type '{}' cannot be instantiated", 1 },
            { "GameStarted", "Game started with GameMode '{}'", 1 },
            { "ClassNotLoadedFallback", "{} class '{}' is not loaded; using {}", 3 },
            { "NoDefaultPlayable", "GameMode has no DefaultPlayable; player {} was not spawned", 1 },
            { "DefaultPlayableWrongType", "The DefaultPlayable {} must be a {}", 2 },
            { "GlfwError", "GLFW error {}: {}", 2 },
            { "PlatformInitFailed", "Failed to initialize the platform layer", 0 },
            { "WindowCreateFailed", "Failed to create window '{}'", 1 },
            { "GameModeMigrated", "Moved the GameMode setting into level '{}'", 1 },
            { "ModuleCopyFailed", "cannot copy module: {}", 1 },
            { "GameModuleLoaded", "Loaded game module '{}' ({} reflected types)", 2 },
            { "ModuleRestored", "Hot reload failed to load the new module; the previous module was restored", 0 },
            { "ModuleRestoreFailed", "Hot reload failed to load the new module, and the previous module could not be restored: {}", 1 },
            { "GameModuleReloaded", "Hot reloaded '{}' (generation {}, {} objects preserved)", 3 },
            { "ModuleMissing", "module '{}' does not exist", 1 },
            { "ProjectExists", "A project already exists in this folder", 0 },
            { "ProjectFoldersFailed", "Cannot create the project folders", 0 },
            { "ProjectWriteFailed", "Cannot write the project file", 0 },
            { "ProjectInvalid", "Invalid project file", 0 },
            { "ManifestInvalid", "invalid game manifest", 0 },
            { "VariantMigrationFailed", "Could not convert '{}' into a {}: {}", 3 },
            { "VariantMigrated", "Converted '{}' into the {} '{}'", 3 },
            { "ShaderIncludeDepth", "include depth limit exceeded", 0 },
            { "ShaderMalformedInclude", "malformed include: {}", 1 },
            { "ShaderIncludeMissing", "cannot open include '{}'", 1 },
            { "ShaderFileMissing", "cannot open shader '{}'", 1 },
            { "SpirvCrossError", "SPIRV-Cross: {}", 1 },
            { "GlslangError", "glslang: {}", 1 },
            { "MetalOnlyOnApple", "Metal shader compilation is only available on Apple platforms", 0 },
            { "ShaderLoadFailed", "Shader '{}': {}", 2 },
            { "ShaderCompileFailed", "Shader compile failed for {}:\n{}", 2 },
            { "PipelineFailed", "Pipeline '{}': {}", 2 },
            { "CorruptCookedTexture", "corrupt cooked texture", 0 },
            { "StbImageError", "stb_image: {}", 1 },
            { "ModelHasNoMeshes", "the file contains no meshes", 0 },
            { "UfbxError", "ufbx: {}", 1 },
            { "CorruptCookedModel", "corrupt cooked model", 0 },
            { "ConvexHullFallback", "Dynamic bodies need convex MeshColliders; using a convex hull for '{}'", 1 },
            { "ConvexHullFailed", "Convex hull creation failed: {}", 1 },
            { "MeshColliderFailed", "Mesh collider creation failed: {}", 1 },
            { "StreamingFailed", "Streaming failed: {} {} could not be loaded", 2 },
            { "ReloadComponentMissing", "Component type '{}' no longer exists after reload; its data is preserved", 1 },
            { "ReloadObjectTypeMissing", "Type '{}' no longer exists after reload; '{}' keeps its data as a missing type", 2 },
            { "ReloadTypeFallback", "Type '{}' no longer exists after reload; it continues as '{}'", 2 },
            { "ReloadTypeRemoved", "Type '{}' no longer exists after reload and has no loaded base class; it was removed", 1 },
            { "ObjectIdCollision", "Object id collision for '{}'; assigning a new id", 1 },
            { "ParentCycle", "Cannot parent '{}' to itself or one of its descendants", 1 },
            { "ParentOtherLevel", "Cannot parent '{}' to an object in another level", 1 },
            { "ComponentAbstract", "Component type '{}' is abstract", 1 },
            { "ComponentDropped", "Dropped the '{}' component: '{}' is no longer a component", 2 },
            { "MissingComponentType", "Missing component type '{}'; its data is preserved", 1 },
            { "LinkedAssetMissing", "{} asset {} is missing; the instance is preserved as a placeholder", 2 },
            { "VariantParentInvalid", "A {} can only be based on a {}", 2 },
            { "MissingObjectType", "Missing {} type '{}' for '{}'; its data is preserved", 3 },
            { "TemplateNotPlaceable", "{} assets cannot be placed or spawned; create a Variant from this Template first", 1 },
            { "CollectionLoadFailed", "{} {} could not be loaded", 2 },
            { "LevelAssetNotFound", "Level asset not found", 0 },
            { "LevelWriteFailed", "could not write level asset", 0 },
            { "CorruptCookedJson", "corrupt cooked JSON data", 0 },
            { "DuplicateAssetGuid", "Duplicate asset guid in '{}'; assigning a new guid", 1 },
            { "NoImporter", "no importer for asset type {}", 1 },
            { "CookedWriteFailed", "cannot write cooked file", 0 },
            { "AssetLoadFailed", "Failed to load asset '{}': {}", 2 },
            { "AssetCookFailed", "{}: {}", 2 },
            { "MissingCookedData", "missing cooked data", 0 },
            { "CookedManifestMissing", "Cooked asset manifest is missing", 0 },
            { "AssetWriteFailed", "Failed to write asset '{}'", 1 },
            { "TypeRegisteredTwice", "Reflected type '{}' is registered twice; the newest registration wins", 1 },
            { "MetalTextureFailed", "Metal: failed to create texture '{}'", 1 },
            { "MslCompileFailed", "MSL compile failed: {}\n{}", 2 },
            { "UnknownError", "unknown", 0 },
            { "EntryPointMissing", "entry point '{}' not found", 1 },
            { "PipelineCreateFailed", "pipeline '{}' failed: {}", 2 },
            { "MetalUnavailable", "Metal is not available on this machine", 0 },
            { "DX11BufferFailed", "DX11: failed to create buffer '{}'", 1 },
            { "DX11TextureFailed", "DX11: failed to create texture '{}'", 1 },
            { "D3DCompileFailed", "D3DCompile failed", 0 },
            { "D3DCompileError", "D3DCompile: {}", 1 },
            { "InputLayoutFailed", "input layout creation failed for '{}'", 1 },
            { "DX11DeviceFailed", "DirectX 11 device creation failed", 0 },
            { "BindingNotPropertyOrComponent", "'{}' is neither a property nor a component in '{}'", 2 },
            { "BindingNoProperty", "'{}' does not name a property", 1 },
            { "ClassNotLoaded", "'{}' is not a loaded {} class", 2 },
            { "NotLevelObjectType", "'{}' is not a {} type", 2 },
            { "WorldTypeAsEngineObject", "'{}' lives in the world; create it as a {} or component instead", 2 },
            { "TypeAbstract", "'{}' is abstract and cannot be created", 1 },
            { "NotAComponent", "'{}' cannot be added as a component", 1 },
            { "ProcessStartFailed", "Failed to start process: {}", 1 },
            { "TextFileNotObject", "the text file is not a JSON object", 0 },
            { "NewFolderName", "New Folder", 0 },
            { "NewLevelName", "New Level", 0 },
            { "NewCollectionName", "New Collection", 0 },
            { "NewTemplateName", "New Template", 0 },
            { "NewMaterialName", "New Material", 0 },
            { "NewScreenName", "New Screen", 0 },
            { "NewWidgetName", "New Widget", 0 },
            { "NewStyleSheetName", "New Style", 0 },
            { "CameraName", "{} Camera", 1 },
            { "MissingNamed", "Missing {}", 1 },
            { "MainLevelName", "Main", 0 },
            { "MainCollectionName", "Main", 0 },
        };

        static_assert(std::size(TextInfos) == static_cast<size_t>(EngineText::Count));
    }

    const EngineTextInfo& GetTextInfo(EngineText text)
    {
        return TextInfos[static_cast<size_t>(text)];
    }

    std::optional<EngineText> FindEngineText(std::string_view key)
    {
        for (size_t index = 0; index < std::size(TextInfos); ++index)
        {
            if (key == TextInfos[index].key)
            {
                return static_cast<EngineText>(index);
            }
        }
        return std::nullopt;
    }

    EngineTextTable& EngineTextTable::Get()
    {
        static EngineTextTable* table = new EngineTextTable();
        return *table;
    }

    EngineTextTable::EngineTextTable()
    {
        Reset();
    }

    bool EngineTextTable::SetOverride(EngineText text, std::string value)
    {
        if (CountFormatPlaceholders(value) != GetTextInfo(text).argumentCount)
        {
            return false;
        }
        m_texts[static_cast<size_t>(text)] = std::move(value);
        return true;
    }

    void EngineTextTable::Reset()
    {
        for (size_t index = 0; index < m_texts.size(); ++index)
        {
            m_texts[index] = TextInfos[index].english;
        }
    }
}
