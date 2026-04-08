#include "KH_Panel.h"
#include "KH_Editor.h"
#include "Scene/KH_Scene.h"
#include "Utils/KH_DebugUtils.h"



std::vector<KH_LOG_MESSAGE> KH_Console::LogMessages;

void KH_Console::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Console");

    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(LogMessages.size()));

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const auto& Log = LogMessages[i];
                ImVec4 logColor;
                bool useSpecificColor = true;

                switch (Log.Flag) {
                case KH_LOG_FLAG::Temp:
                    logColor = ImVec4(0.5f, 0.8f, 1.0f, 1.0f); // 浅天蓝色
                    break;
                case KH_LOG_FLAG::Debug:
                    logColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // 翠绿色
                    break;
                case KH_LOG_FLAG::Warning:
                    logColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // 亮黄色/橙色
                    break;
                case KH_LOG_FLAG::Error:
                    logColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // 亮红色
                    break;
                default:
                    useSpecificColor = false;
                    break;
                }

                if (useSpecificColor) {
                    ImGui::TextColored(logColor, "%s", Log.Message.c_str());
                }
                else {
                    ImGui::TextUnformatted(Log.Message.c_str());
                }
                ImGui::Separator();
            }
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();

    ImGui::End();
    ImGui::PopStyleVar();
}

void KH_Insepctor::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Inspector");

    KH_Editor& Editor = KH_Editor::Instance();

    int32_t selected = Editor.GetSelectedObjectID();
    auto& objects = Editor.Scene.GetObjects();

    if (selected < 0 || selected >= static_cast<int32_t>(objects.size()))
    {
        ImGui::Indent(20.0f);
        ImGui::TextDisabled("No selection");
        ImGui::Unindent(20.0f);
    }
    else
    {
        KH_Object* Object = objects[selected].get();
        if (Object)
        {
            KH_InspectorEditResult EditResult = Object->DrawInspector();
            if (EditResult.CommitType == KH_InspectorCommitType::RebuildBVH)
            {
                Editor.RequestSceneRebuild();
            }
        }
    }



    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();
     
    ImGui::End();
    ImGui::PopStyleVar();

}

void KH_GlobalInfo::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("GlobalInfo");

    {
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "(%.2f ms)", 1000.0f / ImGui::GetIO().Framerate);

            static bool vsync = true;
            if (ImGui::Checkbox("V-Sync", &vsync)) {
                glfwSwapInterval(vsync ? 1 : 0);
            }
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::BulletText("GPU: %s", glGetString(GL_RENDERER));
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::Text("Canvas: %dx%d", KH_Editor::GetCanvasWidth(), KH_Editor::GetCanvasHeight());
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent(20.0f);
            ImGui::Text("Yaw  : %.2f", KH_Editor::Instance().Camera.Yaw);
            ImGui::Text("Pitch: %.2f", KH_Editor::Instance().Camera.Pitch);
            ImGui::Text("Speed: %.2f", KH_Editor::Instance().Camera.MovementSpeed);
            ImGui::Text("Fovy : %.2f", KH_Editor::Instance().Camera.Fovy);
            ImGui::Unindent(20.0f);
        }

        ImGui::Separator();
    }

    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();

    ImGui::End();
    ImGui::PopStyleVar();
}

void KH_SceneTree::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("SceneTree");

    KH_Editor& Editor = KH_Editor::Instance();
    auto& objects = Editor.Scene.GetObjects();

    const int selectedModelID = Editor.GetSelectedObjectID();
    const int selectedMeshID = Editor.GetSelectedObjectMeshID();

    ImGui::Indent(10.0f);
    ImGui::Text("Models: %d", static_cast<int>(objects.size()));
    ImGui::SameLine();
    ImGui::TextDisabled("| Materials: %d", static_cast<int>(Editor.Scene.Materials.size()));
    ImGui::Unindent(10.0f);

    ImGui::Separator();

    if (objects.empty())
    {
        ImGui::Indent(20.0f);
        ImGui::TextDisabled("Empty scene");
        ImGui::Unindent(20.0f);
    }
    else
    {
        for (int modelID = 0; modelID < static_cast<int>(objects.size()); ++modelID)
        {
            KH_Model* model = dynamic_cast<KH_Model*>(objects[modelID].get());
            if (!model)
                continue;

            const auto& meshes = model->GetMeshes();
            const bool isModelSelected = (selectedModelID == modelID && selectedMeshID == 0);
            const bool isThisModelActive = (selectedModelID == modelID);

            // 当前选中的 Model 始终展开
            ImGui::SetNextItemOpen(isThisModelActive, ImGuiCond_Always);

            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_OpenOnArrow |
                ImGuiTreeNodeFlags_SpanAvailWidth;

            if (isModelSelected)
                flags |= ImGuiTreeNodeFlags_Selected;

            char modelLabel[128];
            std::snprintf(
                modelLabel,
                sizeof(modelLabel),
                "Model [%d]  (%d Meshes)",
                modelID,
                static_cast<int>(meshes.size())
            );

            bool open = ImGui::TreeNodeEx((void*)(intptr_t)modelID, flags, "%s", modelLabel);

            // 点击 Model：选中该 Model，并把 MeshID 设为 0
            if (ImGui::IsItemClicked())
            {
                Editor.SetSelectedObjectID(modelID, 0);
            }

            if (open)
            {
                ImGui::Indent(20.0f);
                for (int meshID = 0; meshID < static_cast<int>(meshes.size()); ++meshID)
                {
                    const bool isMeshSelected =
                        (selectedModelID == modelID && selectedMeshID == meshID);

                    char meshLabel[128];
                    std::snprintf(
                        meshLabel,
                        sizeof(meshLabel),
                        "Mesh [%d]",
                        meshID
                    );

                    if (ImGui::Selectable(meshLabel, isMeshSelected))
                    {
                        Editor.SetSelectedObjectID(modelID, meshID);
                    }
                }
                ImGui::Unindent(20.0f);
                ImGui::TreePop();
            }
        }
    }

    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();

    ImGui::End();
    ImGui::PopStyleVar();
}


