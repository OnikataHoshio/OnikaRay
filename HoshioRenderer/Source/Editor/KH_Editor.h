#pragma once
#include "KH_Camera.h"
#include "KH_Window.h"
#include "KH_Canvas.h"

#include "KH_MaterialEditor.h"
#include "Scene/KH_Scene.h"


class KH_SceneBase;

enum class KH_GizmoOperation
{
    Translate,
    Rotate,
    Scale
};

enum class KH_GizmoMode
{
    Local,
    World
};

class KH_Editor
{
public:
    static KH_Editor& Instance();

    KH_Editor(const KH_Editor&) = delete;
    KH_Editor& operator=(const KH_Editor&) = delete;

    void BeginRender();
    void Render();
    void EndRender();

    void UpdateCanvasExtent(uint32_t Width, uint32_t Height);
    void BindCanvasFramebuffer();
    void UnbindCanvasFramebuffer();

    const KH_Framebuffer& GetLastFramebuffer();

    int GetSelectedObjectID() const;
    int GetSelectedObjectMeshID() const;

    uint32_t GetFrameCounter() const;

    void RequestSceneRebuild();
    void RequestFrameReset();

    KH_Canvas& GetCanvas();

    static void SetEditorWidth(uint32_t Width);
    static void SetEditorHeight(uint32_t Height);
    static void SetCanvasWidth(uint32_t Width);
    static void SetCanvasHeight(uint32_t Height);
    static void SetTitle(std::string Title);
    static void SetDefaultScenePath(std::string ScenePath);

    static uint32_t GetEditorWidth();
    static uint32_t GetEdtiorHeight();
    static uint32_t GetCanvasWidth();
    static uint32_t GetCanvasHeight();
    static const std::string& GetTitle();
    static const std::string& GetDefaultScenePath();

    GLFWwindow* GLFWwindow() const;

    // 在 Canvas 窗口内部调用
    void DrawCanvasGizmos();

    void SetSelectedObjectID(int ObjectID, int MeshID = 0);
    bool DeleteSelectedObject(bool OnlyDeleteModel = true);

    bool AddExternalModelFromFile(const std::string& filePath, int materialSlotID = 0);
    bool AddBuiltinModel(int builtinTypeIndex, float size = 1.0f, int materialSlotID = 0);

    KH_Camera Camera;
    KH_Window Window;
    KH_GpuLBVHScene Scene;

private:
    static uint32_t EditorWidth;
    static uint32_t EditorHeight;
    static uint32_t CanvasWidth;
    static uint32_t CanvasHeight;
    static std::string Title;
    static std::string DefaultScenePath;

    uint32_t FrameCounter = 0;
    int SelectedObjectID = -1;
    int SelectedObjectMeshID = -1;

    bool bSceneRebuildRequested = false;
    bool bFrameResetRequested = false;

    bool bGizmoOver = false;
    bool bGizmoUsing = false;

    bool bViewManipulatorHovered = false;
    bool bViewManipulatorUsing = false;

    KH_GizmoOperation GizmoOperation = KH_GizmoOperation::Translate;
    KH_GizmoMode GizmoMode = KH_GizmoMode::Local;

    bool bUseGizmoSnap = false;
    glm::vec3 TranslateSnap = glm::vec3(0.1f);
    float RotateSnap = 15.0f;
    float ScaleSnap = 0.1f;

    float ViewManipulatorSize = 96.0f;
    float ViewManipulatorMargin = 12.0f;
    ImU32 ViewManipulatorBgColor = 0x10101010;

    KH_Canvas Canvas;
    KH_Console Console;
    KH_Insepctor Inspector;
    KH_GlobalInfo GlobalInfo;
    KH_SceneTree SceneTree;
    KH_MaterialEditor MaterialsEditor;

    std::string CurrentSceneXmlPath;


    KH_Editor();
    ~KH_Editor();

    void Initialize();
    void DeInitialize();

    void RenderDockSpace();

    void BeginImgui();
    void EndImgui();

    void ResetFrameCounter();

    void UpdateSelectedObjectID();

    void DrawObjectGizmo();
    void DrawViewManipulator();
    void ApplyViewManipulatorToCamera(const glm::mat4& manipulatedView, const glm::vec3& pivot, float distance);
    glm::vec3 GetViewManipulatorPivot() const;

    void DrawMainMenuBar();
    void OpenImportSceneDialog();
    bool ImportSceneFromFile(const std::string& filePath);

    bool SaveSceneToFile(const std::string& filePath);
    bool SaveSceneAs();
    bool SaveScene();

    void NewScene();

    void OpenImportModelDialog();
    void DrawCanvasContextMenu();

    int EnsureUsableMaterialSlot(int requestedSlot);
    void InitializeSpawnedObjectTransform(KH_Object& object);
};



