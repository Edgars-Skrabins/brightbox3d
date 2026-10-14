#include "Panels/Assets/AssetFileImport.h"

#include "Assets/AssetDatabase.h"
#include "Core/Log.h"
#include "EditorText.h"

namespace BrightBox3D::Editor::AssetFileImport
{
    namespace
    {
        std::filesystem::path Destination(Assets::AssetDatabase& database, const std::filesystem::path& file, const std::string& folder)
        {
            std::filesystem::path destination = database.GetAssetsDirectory() / folder / file.filename();
            if (std::filesystem::exists(destination))
            {
                destination = database.GetAssetsDirectory() / database.MakeUniquePath((folder.empty() ? "" : folder + "/") + file.filename().string());
            }
            return destination;
        }

        void CopyInto(const std::filesystem::path& file, const std::filesystem::path& destination, std::error_code& error)
        {
            if (std::filesystem::is_directory(file))
            {
                std::filesystem::copy(file, destination, std::filesystem::copy_options::recursive, error);
            }
            else
            {
                std::filesystem::copy_file(file, destination, error);
            }
        }
    }

    void ImportFiles(const std::vector<std::filesystem::path>& files, const std::string& folder)
    {
        Assets::AssetDatabase& database = Assets::AssetDatabase::Get();
        if (!database.IsOpen())
        {
            return;
        }
        for (const std::filesystem::path& file : files)
        {
            std::error_code error;
            std::filesystem::path destination = Destination(database, file, folder);
            CopyInto(file, destination, error);
            if (error)
            {
                Core::Log::Error(Text(EditorText::ImportFailed, file.generic_string(), error.message()));
            }
            else
            {
                Core::Log::Info(Text(EditorText::Imported, destination.filename().string()));
            }
        }
        database.Refresh();
    }
}
