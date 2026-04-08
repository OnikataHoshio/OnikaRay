#include "KH_Editor.h"
#include "Hit/KH_Ray.h"
#include "Scene/KH_Scene.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "pfd/portable-file-dialogs.h"


uint32_t KH_Editor::EditorWidth = 1920;
uint32_t KH_Editor::EditorHeight = 1080;
uint32_t KH_Editor::CanvasWidth = 800;
uint32_t KH_Editor::CanvasHeight = 800;
std::string KH_Editor::Title = "KH_Renderer";
std::string KH_Editor::DefaultScenePath = "";

namespace
{
    ImGuizmo::OPERATION ToImGuizmoOperation(KH_GizmoOperation op)
    {
        switch (op)
        {
        case KH_GizmoOperation::Translate: return ImGuizmo::TRANSLATE;
        case KH_GizmoOperation::Rotate:    return ImGuizmo::ROTATE;
        case KH_GizmoOperation::Scale:     return ImGuizmo::SCALE;
        default:                           return ImGuizmo::TRANSLATE;
        }
    }

    ImGuizmo::MODE ToImGuizmoMode(KH_GizmoMode mode)
    {
        switch (mode)
        {
        case KH_GizmoMode::Local: return ImGuizmo::LOCAL;
        case KH_GizmoMode::World: return ImGuizmo::WORLD;
        default:                  return ImGuizmo::LOCAL;
        }
    }

    bool ExtractTRS(const glm::mat4& m, glm::vec3& outPosition, glm::quat& outRotation, glm::vec3& outScale)
    {
        outPosition = glm::vec3(m[3]);

        const glm::vec3 col0 = glm::vec3(m[0]);
        const glm::vec3 col1 = glm::vec3(m[1]);
        const glm::vec3 col2 = glm::vec3(m[2]);

        outScale.x = glm::length(col0);
        outScale.y = glm::length(col1);
        outScale.z = glm::length(col2);

        if (outScale.x <= 1e-6f || outScale.y <= 1e-6f || outScale.z <= 1e-6f)
            return false;

        glm::mat3 rot;
        rot[0] = col0 / outScale.x;
        rot[1] = col1 / outScale.y;
        rot[2] = col2 / outScale.z;

        outRotation = glm::normalize(glm::quat_cast(rot));
        return true;
    }
}

namespace
{
    std::string EnsureXmlExtension(std::string path)
    {
        if (path.empty())
            return path;

        std::filesystem::path p(path);
        if (!p.has_extension())
            p.replace_extension(".xml");

        return p.string();
    }
}

KH_Editor& KH_Editor::Instance()
{
    static KH_Editor instance;
    return instance;
}

uint32_t KH_Editor::GetFrameCounter() const
{
    return FrameCounter;
}

void KH_Editor::RequestSceneRebuild()
{
    bSceneRebuildRequested = true;
    RequestFrameReset();
}

void KH_Editor::RequestFrameReset()
{
    bFrameResetRequested = true;
}

KH_Canvas& KH_Editor::GetCanvas()
{
    return Canvas;
}

void KH_Editor::SetEditorWidth(uint32_t Width)
{
    EditorWidth = Width;
}

void KH_Editor::SetEditorHeight(uint32_t Height)
{
    EditorHeight = Height;
}

void KH_Editor::SetCanvasWidth(uint32_t Width)
{
    CanvasWidth = Width;
}

void KH_Editor::SetCanvasHeight(uint32_t Height)
{
    CanvasHeight = Height;
}

void KH_Editor::SetTitle(std::string Title)
{
    KH_Editor::Title = Title;
}

void KH_Editor::SetDefaultScenePath(std::string ScenePath)
{
    KH_Editor::DefaultScenePath = ScenePath;
}

const std::string& KH_Editor::GetDefaultScenePath()
{
    return DefaultScenePath;
    return DefaultScenePath;
}

uint32_t KH_Editor::GetEditorWidth()
{
    return EditorWidth;
}

uint32_t KH_Editor::GetEdtiorHeight()
{
    return EditorHeight;
}

uint32_t KH_Editor::GetCanvasWidth()
{
    return CanvasWidth;
}

uint32_t KH_Editor::GetCanvasHeight()
{
    return CanvasHeight;
}

const std::string& KH_Editor::GetTitle()
{
    return Title;
}

GLFWwindow* KH_Editor::GLFWwindow() const
{
    return Window.Window;
}

void KH_Editor::BeginRender()
{
    bSceneRebuildRequested = false;
    bFrameResetRequested = false;
    Window.BeginRender();
    BeginImgui();
    RenderDockSpace();
}

void KH_Editor::Render()
{
    Scene.Render();
}

void KH_Editor::EndRender()
{
    Canvas.Render();
    DrawCanvasContextMenu();
    UpdateSelectedObjectID();

    Console.Render();
    SceneTree.Render();
    Inspector.Render();
    MaterialsEditor.Render();
    GlobalInfo.Render();
    Canvas.SwapFramebuffer();
    EndImgui();
    Window.EndRender();

    FrameCounter += 1;

    if (bSceneRebuildRequested)
    {
        Scene.BindAndBuild();
    }

    if (bFrameResetRequested)
    {
        ResetFrameCounter();
    }
}

void KH_Editor::UpdateCanvasExtent(uint32_t Width, uint32_t Height)
{
    CanvasWidth = Width;
    CanvasHeight = Height;

    Camera.Width = Width;
    Camera.Height = Height;
    Camera.UpdateAspect();

    RequestFrameReset();
}

void KH_Editor::BindCanvasFramebuffer()
{
    Canvas.BindSceneFramebuffer();
}

void KH_Editor::UnbindCanvasFramebuffer()
{
    Canvas.UnbindSceneFramebuffer();
}

const KH_Framebuffer& KH_Editor::GetLastFramebuffer()
{
    return Canvas.GetLastFramebuffer();
}

int KH_Editor::GetSelectedObjectID() const
{
    return SelectedObjectID;
}

int KH_Editor::GetSelectedObjectMeshID() const
{
    return SelectedObjectMeshID;
}

void KH_Editor::ResetFrameCounter()
{
    FrameCounter = 0;
}

KH_Editor::KH_Editor()
    :Camera(CanvasWidth, CanvasHeight), Window(EditorWidth, EditorHeight, Title)
{
    Window.SetCamera(&Camera);
    Initialize();
}

KH_Editor::~KH_Editor()
{
    DeInitialize();
}

void KH_Editor::Initialize()
{
    ImportSceneFromFile(DefaultScenePath);
}

void KH_Editor::DeInitialize()
{
}

void KH_Editor::RenderDockSpace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("RootDockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    DrawMainMenuBar();

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    ImGui::End();
}

void KH_Editor::BeginImgui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void KH_Editor::EndImgui()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void KH_Editor::UpdateSelectedObjectID()
{
    if (ImGuizmo::IsUsingAny() || ImGuizmo::IsViewManipulateHovered())
        return;

    KH_Ray pickRay;
    if (Canvas.TryBuildPickRay(Camera, pickRay, 0))
    {
        KH_PickResult result = Scene.Pick(pickRay);
        SelectedObjectID = result.bIsHit ? result.ObjectIndex : -1;
        SelectedObjectMeshID = result.ObjectMeshID;
    }
}

void KH_Editor::DrawCanvasGizmos()
{
    if (!Canvas.IsHovered() && !Canvas.IsFocused())
        return;

    const glm::vec2& min = Canvas.GetCanvasMin();
    const glm::vec2& size = Canvas.GetCanvasSize();
    if (size.x <= 0.0f || size.y <= 0.0f)
        return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(min.x, min.y, size.x, size.y);

    DrawViewManipulator();
    DrawObjectGizmo();
}

void KH_Editor::SetSelectedObjectID(int ObjectID, int MeshID)
{
    SelectedObjectID = ObjectID;
    SelectedObjectMeshID = MeshID;
}

bool KH_Editor::DeleteSelectedObject(bool OnlyDeleteModel)
{
    auto& objects = Scene.GetObjects();
    if (SelectedObjectID < 0 || SelectedObjectID >= static_cast<int32_t>(objects.size()))
        return false;

    KH_Object* object = objects[SelectedObjectID].get();
    if (!object)
        return false;

    if (OnlyDeleteModel && dynamic_cast<KH_Model*>(object) == nullptr)
        return false;

    if (!Scene.RemoveObjectAt(static_cast<size_t>(SelectedObjectID)))
        return false;

    SelectedObjectID = -1;
    RequestSceneRebuild();
    RequestFrameReset();
    return true;
}

bool KH_Editor::AddExternalModelFromFile(const std::string& filePath, int materialSlotID)
{
    if (filePath.empty())
        return false;

    const int slot = EnsureUsableMaterialSlot(materialSlotID);

    KH_Model& model = Scene.AddModel(slot, filePath);
    InitializeSpawnedObjectTransform(model);

    SelectedObjectID = static_cast<int32_t>(Scene.GetObjects().size()) - 1;
    RequestSceneRebuild();
    RequestFrameReset();

    return true;
}

bool KH_Editor::AddBuiltinModel(int builtinTypeIndex, float size, int materialSlotID)
{
    const int slot = EnsureUsableMaterialSlot(materialSlotID);

    KH_Model model;
    switch (builtinTypeIndex)
    {
    case 0:
        model = KH_PrimitiveFactory::CreatePlane(size);
        break;
    case 1:
        model = KH_PrimitiveFactory::CreateCube(size);
        break;
    case 2:
        model = KH_PrimitiveFactory::CreateEmptyCube(size);
        break;
    case 3:
        model = KH_PrimitiveFactory::CreateFullscreenQuad(size);
        break;
    default:
        return false;
    }

    KH_Model& modelRef = Scene.AddModel(slot, std::move(model));
    InitializeSpawnedObjectTransform(modelRef);

    SelectedObjectID = static_cast<int32_t>(Scene.GetObjects().size()) - 1;
    RequestSceneRebuild();
    RequestFrameReset();

    return true;
}


glm::vec3 KH_Editor::GetViewManipulatorPivot() const
{
    glm::vec3 pivot = Scene.AABB.GetCenter();

    const auto& objects = Scene.GetObjects();
    if (SelectedObjectID >= 0 &&
        SelectedObjectID < static_cast<int32_t>(objects.size()) &&
        objects[SelectedObjectID])
    {
        pivot = objects[SelectedObjectID]->GetAABB().GetCenter();
    }

    return pivot;
}

void KH_Editor::DrawMainMenuBar()
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        const bool hasCurrentPath = !CurrentSceneXmlPath.empty();

        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
        {
            NewScene();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Add"))
        {
            if (ImGui::MenuItem("Import External Model..."))
            {
                OpenImportModelDialog();
            }

            if (ImGui::BeginMenu("Builtin Model"))
            {
                if (ImGui::MenuItem("Plane"))
                    AddBuiltinModel(0, 1.0f, 0);

                if (ImGui::MenuItem("Cube"))
                    AddBuiltinModel(1, 1.0f, 0);

                if (ImGui::MenuItem("EmptyCube"))
                    AddBuiltinModel(2, 1.0f, 0);

                if (ImGui::MenuItem("FullscreenQuad"))
                    AddBuiltinModel(3, 1.0f, 0);

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Import Scene...", "Ctrl+X"))
        {
            OpenImportSceneDialog();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save", "Ctrl+S", false, hasCurrentPath))
        {
            SaveScene();
        }

        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, true))
        {
            SaveSceneAs();
        }

        ImGui::Separator();

        ImGui::TextDisabled(
            hasCurrentPath ? CurrentSceneXmlPath.c_str() : "No current scene file"
        );

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();

    if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_X))
    {
        OpenImportSceneDialog();
    }

    const ImGuiIO& io = ImGui::GetIO();
    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;

    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        SaveScene();
    }

    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        SaveSceneAs();
    }

    if (ctrl &&  ImGui::IsKeyPressed(ImGuiKey_N, false))
    {
        NewScene();
    }
}

void KH_Editor::OpenImportSceneDialog()
{
    std::vector<std::string> files = pfd::open_file(
        "Import Scene",
        ".",
        { "Scene XML Files", "*.xml", "All Files", "*" }
    ).result();

    if (files.empty())
        return;

    ImportSceneFromFile(files[0]);
}

bool KH_Editor::ImportSceneFromFile(const std::string& filePath)
{
    if (!Scene.LoadFromXml(filePath))
    {
        return false;
    }

    Scene.BindAndBuild();

    CurrentSceneXmlPath = filePath;
    SelectedObjectID = -1;
    RequestFrameReset();

    return true;
}

void KH_Editor::DrawViewManipulator()
{
    glm::mat4 view = Camera.GetViewMatrix();
    glm::mat4 oldView = view;

    const glm::vec2& min = Canvas.GetCanvasMin();
    const glm::vec2& size = Canvas.GetCanvasSize();

    ImVec2 widgetPos(
        min.x + size.x - ViewManipulatorSize - ViewManipulatorMargin,
        min.y + ViewManipulatorMargin
    );
    ImVec2 widgetSize(ViewManipulatorSize, ViewManipulatorSize);

    glm::vec3 pivot = GetViewManipulatorPivot();
    float distance = glm::length(Camera.Position - pivot);
    distance = std::max(distance, 0.001f);

    ImGuizmo::PushID("ViewManipulator");
    ImGuizmo::ViewManipulate(
        glm::value_ptr(view),
        distance,
        widgetPos,
        widgetSize,
        ViewManipulatorBgColor
    );
    bViewManipulatorHovered = ImGuizmo::IsViewManipulateHovered();
    bViewManipulatorUsing = ImGuizmo::IsUsingViewManipulate();
    ImGuizmo::PopID();

    bool viewChanged = std::memcmp(glm::value_ptr(oldView), glm::value_ptr(view), sizeof(float) * 16) != 0;
    if (viewChanged)
    {
        ApplyViewManipulatorToCamera(view, pivot, distance);
        RequestFrameReset();
    }

    if (bViewManipulatorUsing)
    {
        RequestFrameReset();
    }
}

void KH_Editor::ApplyViewManipulatorToCamera(const glm::mat4& manipulatedView, const glm::vec3& pivot, float distance)
{
    glm::mat4 invView = glm::inverse(manipulatedView);

    glm::vec3 right = glm::normalize(glm::vec3(invView[0]));
    glm::vec3 up = glm::normalize(glm::vec3(invView[1]));
    glm::vec3 back = glm::normalize(glm::vec3(invView[2]));
    glm::vec3 front = -back;

    Camera.Right = right;
    Camera.Up = up;
    Camera.Front = glm::normalize(front);

    Camera.Position = pivot - Camera.Front * distance;

    Camera.Pitch = glm::degrees(std::asin(glm::clamp(Camera.Front.y, -1.0f, 1.0f)));
    Camera.Yaw = glm::degrees(std::atan2(Camera.Front.z, Camera.Front.x));
}

void KH_Editor::DrawObjectGizmo()
{
    bGizmoOver = false;
    bGizmoUsing = false;

    const auto& objects = Scene.GetObjects();
    if (SelectedObjectID < 0 || SelectedObjectID >= static_cast<int32_t>(objects.size()))
        return;

    KH_Object* object = objects[SelectedObjectID].get();
    if (!object)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_G)) GizmoOperation = KH_GizmoOperation::Translate;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) GizmoOperation = KH_GizmoOperation::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_Y)) GizmoOperation = KH_GizmoOperation::Scale;
    if (ImGui::IsKeyPressed(ImGuiKey_L)) GizmoMode = KH_GizmoMode::Local;
    if (ImGui::IsKeyPressed(ImGuiKey_W)) GizmoMode = KH_GizmoMode::World;
    if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::IsKeyPressed(ImGuiKey_LeftShift)) bUseGizmoSnap = !bUseGizmoSnap;

    glm::mat4 view = Camera.GetViewMatrix();
    glm::mat4 proj = Camera.GetProjMatrix();
    glm::mat4 gizmoMatrix = object->GetGizmoMatrix();

    float translateSnap[3] = { TranslateSnap.x, TranslateSnap.y, TranslateSnap.z };
    float rotateSnap = RotateSnap;
    float scaleSnap = ScaleSnap;

    const float* snap = nullptr;
    switch (GizmoOperation)
    {
    case KH_GizmoOperation::Translate:
        snap = bUseGizmoSnap ? translateSnap : nullptr;
        break;
    case KH_GizmoOperation::Rotate:
        snap = bUseGizmoSnap ? &rotateSnap : nullptr;
        break;
    case KH_GizmoOperation::Scale:
        snap = bUseGizmoSnap ? &scaleSnap : nullptr;
        break;
    }

    ImGuizmo::PushID("ObjectGizmo");
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        ToImGuizmoOperation(GizmoOperation),
        ToImGuizmoMode(GizmoMode),
        glm::value_ptr(gizmoMatrix),
        nullptr,
        snap
    );
    bGizmoOver = ImGuizmo::IsOver();
    bGizmoUsing = ImGuizmo::IsUsing();
    ImGuizmo::PopID();

    if (bGizmoUsing)
    {
        const glm::vec3& pivotLocal = object->GetGizmoPivotLocal();

        glm::mat4 newModel =
            gizmoMatrix *
            glm::inverse(glm::translate(glm::mat4(1.0f), pivotLocal));

        glm::vec3 position;
        glm::vec3 scale;
        glm::quat rotationQuat;

        if (ExtractTRS(newModel, position, rotationQuat, scale))
        {
            object->SetTransform(position, rotationQuat, scale);
            RequestSceneRebuild();
            RequestFrameReset();
        }
    }
}

bool KH_Editor::SaveSceneToFile(const std::string& filePath)
{

    std::string finalPath = EnsureXmlExtension(filePath);
    if (finalPath.empty())
        return false;

    if (!Scene.SaveToXml(finalPath))
        return false;

    CurrentSceneXmlPath = finalPath;
    return true;
}

bool KH_Editor::SaveSceneAs()
{

    std::string defaultPath = CurrentSceneXmlPath.empty() ? "." : CurrentSceneXmlPath;

    std::string path = pfd::save_file(
        "Save Scene As",
        defaultPath,
        { "Scene XML Files", "*.xml", "All Files", "*" }
    ).result();

    if (path.empty())
        return false;

    return SaveSceneToFile(path);
}

bool KH_Editor::SaveScene()
{

    if (!CurrentSceneXmlPath.empty())
        return SaveSceneToFile(CurrentSceneXmlPath);

    return SaveSceneAs();
}

void KH_Editor::NewScene()
{
    Scene.Clear();
    Scene.Materials.clear();

    KH_BRDFMaterial defaultMat;
    defaultMat.BaseColor = glm::vec3(0.8f);
    Scene.Materials.push_back(defaultMat);

    Scene.BindAndBuild();

    CurrentSceneXmlPath.clear();
    SelectedObjectID = -1;
    RequestFrameReset();
}

int KH_Editor::EnsureUsableMaterialSlot(int requestedSlot)
{
    if (Scene.Materials.empty())
    {
        KH_BRDFMaterial defaultMat;
        defaultMat.BaseColor = glm::vec3(0.8f);
        Scene.Materials.push_back(defaultMat);
    }

    if (requestedSlot < 0)
        return 0;

    if (requestedSlot >= static_cast<int>(Scene.Materials.size()))
        return static_cast<int>(Scene.Materials.size()) - 1;

    return requestedSlot;
}

void KH_Editor::InitializeSpawnedObjectTransform(KH_Object& object)
{
    //const glm::vec3 spawnPos = Camera.Position + Camera.Front * 3.0f;
    //object.SetPosition(spawnPos);
    object.SetPosition(glm::vec3(0.0f));
}

void KH_Editor::OpenImportModelDialog()
{
    std::vector<std::string> files = pfd::open_file(
        "Import Model",
        ".",
        {
            "Model Files", "*.obj *.fbx *.gltf *.glb *.dae *.3ds",
            "All Files", "*"
        }
    ).result();

    if (files.empty())
        return;

    AddExternalModelFromFile(files[0], 0);
}

void KH_Editor::DrawCanvasContextMenu()
{
    if (Canvas.IsHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_A))
    {
        ImGui::OpenPopup("CanvasContextMenu");
    }

    if (ImGui::BeginPopup("CanvasContextMenu"))
    {
        if (ImGui::MenuItem("Import External Model..."))
        {
            OpenImportModelDialog();
        }

        if (ImGui::BeginMenu("Add Builtin Model"))
        {
            if (ImGui::MenuItem("Plane"))
                AddBuiltinModel(0, 1.0f, 0);

            if (ImGui::MenuItem("Cube"))
                AddBuiltinModel(1, 1.0f, 0);

            if (ImGui::MenuItem("EmptyCube"))
                AddBuiltinModel(2, 1.0f, 0);

            if (ImGui::MenuItem("FullscreenQuad"))
                AddBuiltinModel(3, 1.0f, 0);

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Delete Selected Model"))
        {
            DeleteSelectedObject(true);
        }

        ImGui::EndPopup();
    }
}