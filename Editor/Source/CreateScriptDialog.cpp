#include "CreateScriptDialog.h"

#include "Core/Log.h"
#include "EditorApplication.h"
#include "EditorPreferences.h"
#include "EditorText.h"
#include "HotReload.h"
#include "Labels.h"
#include "Panels/AssetsPanel.h"
#include "Reflection/TypeInfo.h"
#include "Runtime/ProjectLayout.h"
#include "Scripts/ScriptFiles.h"
#include "Services/ScriptLibrary.h"
#include "Theme.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace BrightBox3D::Editor
{
    namespace
    {
        constexpr const char* PopupId = "CreateScript";
        constexpr float TemplateListExtraRows = 0.5f;

        std::string FolderLabel(const std::string& folder)
        {
            std::string root = Runtime::ProjectLayout::ScriptsFolder;
            return folder.empty() ? root : root + "/" + folder;
        }

        void SelectInAssets(EditorApplication& application, const std::string& folder, const std::string& className)
        {
            AssetsPanel* assets = application.GetAssets();
            const ScriptEntry* script = application.GetContext().GetScriptLibrary().FindByClass(className);
            if (!assets || !script)
            {
                return;
            }
            AssetsPanelState& state = assets->GetState();
            state.root = AssetsRoot::Scripts;
            state.scriptFolder = folder;
            state.selectedScript = script->id;
        }
    }

    void CreateScriptDialog::Open(const CreateScriptRequest& request)
    {
        m_request = request;
        m_name.clear();
        m_error.clear();
        m_openRequested = true;
    }

    void CreateScriptDialog::DrawTemplates()
    {
        ImGui::SeparatorText(Text(EditorText::ScriptTemplateHeading));
        if (!ImGui::BeginListBox("##templates", ImVec2(-FLT_MIN, ImGui::GetTextLineHeightWithSpacing() * (static_cast<float>(ScriptTemplate::Count) + TemplateListExtraRows))))
        {
            return;
        }
        for (size_t index = 0; index < static_cast<size_t>(ScriptTemplate::Count); ++index)
        {
            ScriptTemplate scriptTemplate = static_cast<ScriptTemplate>(index);
            const ScriptTemplateInfo& info = GetScriptTemplateInfo(scriptTemplate);
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Selectable(IconLabel(ScriptTemplateIcon(scriptTemplate), Text(info.label)).c_str(), m_request.scriptTemplate == scriptTemplate))
            {
                m_request.scriptTemplate = scriptTemplate;
            }
            std::string basedOn = Text(EditorText::ScriptBasedOn, info.baseType().GetName());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(basedOn.c_str()).x);
            ImGui::TextColored(EditorColorVec4(EditorColor::TextDim), "%s", basedOn.c_str());
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    void CreateScriptDialog::DrawFolders(EditorApplication& application)
    {
        ImGui::SeparatorText(Text(EditorText::ScriptFolderHeading));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!ImGui::BeginCombo("##folder", FolderLabel(m_request.folder).c_str()))
        {
            return;
        }
        if (ImGui::Selectable(FolderLabel({}).c_str(), m_request.folder.empty()))
        {
            m_request.folder.clear();
        }
        for (const std::string& folder : application.GetContext().GetScriptLibrary().GetFolders())
        {
            if (ImGui::Selectable(FolderLabel(folder).c_str(), m_request.folder == folder))
            {
                m_request.folder = folder;
            }
        }
        ImGui::EndCombo();
    }

    bool CreateScriptDialog::Create(EditorApplication& application)
    {
        ScriptLibrary& library = application.GetContext().GetScriptLibrary();
        if (Core::Result<void, EditorError> valid = ScriptFiles::ValidateNewClassName(m_name, library); !valid)
        {
            m_error = valid.GetError().GetText();
            return false;
        }
        Core::Result<std::filesystem::path, EditorError> source = ScriptFiles::CreateScript(library.GetRoot(), m_request.folder, m_name, m_request.scriptTemplate);
        if (!source)
        {
            m_error = source.GetError().GetText();
            return false;
        }
        library.Refresh();
        Core::Log::Info(Text(EditorText::ScriptCreated, m_name));
        if (m_request.targetObject.IsValid() && m_request.scriptTemplate == ScriptTemplate::Component)
        {
            application.GetPendingScriptComponent().Request(m_request.targetObject, m_name);
        }
        SelectInAssets(application, m_request.folder, m_name);
        application.OpenFileInEditor(source->string(), 0);
        application.GetHotReload()->RequestCompile();
        return true;
    }

    void CreateScriptDialog::Draw(EditorApplication& application)
    {
        std::string title = LabelWithId(EditorText::CreateScriptTitle, PopupId);
        if (m_openRequested)
        {
            ImGui::OpenPopup(title.c_str());
            m_openRequested = false;
        }
        if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(ScaledMetric(EditorMetric::CreateScriptDialogWidth));
        bool submitted = ImGui::InputTextWithHint("##name", Text(EditorText::ScriptClassName), &m_name, ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsItemEdited())
        {
            m_error.clear();
        }
        DrawTemplates();
        DrawFolders(application);
        if (!m_error.empty())
        {
            ImGui::TextColored(EditorColorVec4(EditorColor::Error), "%s", m_error.c_str());
        }
        ImGui::Spacing();
        ImGui::BeginDisabled(m_name.empty());
        submitted |= ImGui::Button(Text(EditorText::Create), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0));
        ImGui::EndDisabled();
        if (submitted && !m_name.empty() && Create(application))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text(EditorText::Cancel), ImVec2(ScaledMetric(EditorMetric::DialogButtonWidth), 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
