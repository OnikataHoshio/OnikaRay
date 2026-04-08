#include "KH_MaterialEditor.h"
#include "KH_Editor.h"

void KH_MaterialEditor::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::Begin("Materials");

    SyncSelectedMaterialFromEditorSelection();

    KH_Editor& Editor = KH_Editor::Instance();
    KH_GpuLBVHScene& Scene = Editor.Scene;

    if (ImGui::Button("New Material"))
    {
        KH_BRDFMaterial mat{};
        mat.BaseColor = glm::vec3(0.8f);
        mat.Emissive = glm::vec3(0.0f);
        mat.Subsurface = 0.0f;
        mat.Metallic = 0.0f;
        mat.Specular = 0.5f;
        mat.SpecularTint = 0.0f;
        mat.Roughness = 0.5f;
        mat.Anisotropic = 0.0f;
        mat.Sheen = 0.0f;
        mat.SheenTint = 0.0f;
        mat.Clearcoat = 0.0f;
        mat.ClearcoatGloss = 0.0f;
        mat.IOR = 1.5f;
        mat.Transmission = 0.0f;

        SelectedMaterialID = Scene.AddMaterial(mat);
        Scene.UpdateMaterialSSBO();
        Editor.RequestFrameReset();
    }

    ImGui::SameLine();

    bool canDelete = !Scene.Materials.empty() &&
        SelectedMaterialID >= 0 &&
        SelectedMaterialID < static_cast<int>(Scene.Materials.size());

    ImGui::BeginDisabled(!canDelete);
    if (ImGui::Button("Delete Material"))
    {
        if (Scene.DeleteMaterial(SelectedMaterialID))
        {
            if (Scene.Materials.empty())
                SelectedMaterialID = -1;
            else
                SelectedMaterialID = std::clamp(
                    SelectedMaterialID,
                    0,
                    static_cast<int>(Scene.Materials.size()) - 1
                );

            // 删除材质后，Primitive 的 MaterialSlotID 映射也变了，所以要整体重建
            Editor.RequestSceneRebuild();
            Editor.RequestFrameReset();
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    if (Scene.Materials.empty())
    {
        ImGui::TextDisabled("No materials");
    }
    else
    {
        SelectedMaterialID = std::clamp(
            SelectedMaterialID,
            0,
            static_cast<int>(Scene.Materials.size()) - 1
        );

        std::vector<std::string> labels;
        labels.reserve(Scene.Materials.size());
        std::vector<const char*> items;
        items.reserve(Scene.Materials.size());

        for (int i = 0; i < static_cast<int>(Scene.Materials.size()); ++i)
        {
            labels.push_back("Material " + std::to_string(i));
        }
        for (auto& s : labels)
        {
            items.push_back(s.c_str());
        }

        ImGui::Text("Selected Material");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##MaterialSelect", &SelectedMaterialID, items.data(), static_cast<int>(items.size()));

        ImGui::Separator();

        KH_BRDFMaterial& Mat = Scene.Materials[SelectedMaterialID];
        bool materialChanged = false;

        materialChanged |= ImGui::ColorEdit3(
            "BaseColor",
            &Mat.BaseColor.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR
        );

        materialChanged |= ImGui::ColorEdit3(
            "Emissive",
            &Mat.Emissive.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR
        );

        materialChanged |= ImGui::DragFloat("Subsurface", &Mat.Subsurface, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("Metallic", &Mat.Metallic, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("Specular", &Mat.Specular, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("SpecularTint", &Mat.SpecularTint, 0.01f, 0.0f, 1.0f);

        materialChanged |= ImGui::DragFloat("Roughness", &Mat.Roughness, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("Anisotropic", &Mat.Anisotropic, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("Sheen", &Mat.Sheen, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("SheenTint", &Mat.SheenTint, 0.01f, 0.0f, 1.0f);

        materialChanged |= ImGui::DragFloat("Clearcoat", &Mat.Clearcoat, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("ClearcoatGloss", &Mat.ClearcoatGloss, 0.01f, 0.0f, 1.0f);
        materialChanged |= ImGui::DragFloat("IOR", &Mat.IOR, 0.01f, 1.0f, 3.0f);
        materialChanged |= ImGui::DragFloat("Transmission", &Mat.Transmission, 0.01f, 0.0f, 1.0f);

        if (materialChanged)
        {
            Scene.UpdateMaterialSSBO();
            Editor.RequestFrameReset();
        }
    }

    bIsFocused = ImGui::IsWindowFocused();
    bIsHovered = ImGui::IsWindowHovered();

    ImGui::End();
    ImGui::PopStyleVar();
}

void KH_MaterialEditor::SyncSelectedMaterialFromEditorSelection()
{
    KH_Editor& Editor = KH_Editor::Instance();
    KH_GpuLBVHScene& Scene = Editor.Scene;

    const int objectID = Editor.GetSelectedObjectID();
    const int meshID = Editor.GetSelectedObjectMeshID();

    // 只有当选中的 object / mesh 真的变了，才同步一次
    if (objectID == LastSyncedObjectID && meshID == LastSyncedMeshID)
        return;

    LastSyncedObjectID = objectID;
    LastSyncedMeshID = meshID;

    if (Scene.Materials.empty())
    {
        SelectedMaterialID = -1;
        return;
    }

    auto& objects = Scene.GetObjects();
    if (objectID < 0 || objectID >= static_cast<int>(objects.size()))
        return;

    KH_Object* object = objects[objectID].get();
    KH_Model* model = dynamic_cast<KH_Model*>(object);
    if (model == nullptr)
        return;

    const auto& meshes = model->GetMeshes();
    if (meshID < 0 || meshID >= static_cast<int>(meshes.size()))
        return;

    SelectedMaterialID = std::clamp(
        meshes[meshID].GetMaterialSlotID(),
        0,
        static_cast<int>(Scene.Materials.size()) - 1
    );
}
