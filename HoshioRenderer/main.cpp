#include "Editor/KH_Editor.h"
#include "Pipeline/KH_Shader.h"
#include "Scene/KH_Scene.h"
#include "Pipeline/RenderGraph/KH_PostProcessGraph.h"
#include "Utils/KH_Algorithms.h"

int main()
{
	KH_Editor::SetEditorWidth(1480);
	KH_Editor::SetEditorHeight(920);
	KH_Editor::SetTitle("KH_Renderer");
	KH_Editor::SetDefaultScenePath("Assert/Scenes/Bunny.xml");
	KH_Editor::Instance();

	KH_Model& Bunny = KH_DefaultModels::Instance().Bunny;
	KH_Shader& TestShader = KH_ExampleShaders::Instance().TestShader;
	KH_Shader& AABBShader = KH_ExampleShaders::Instance().AABBShader;

	glEnable(GL_DEPTH_TEST);
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	KH_Editor& Editor = KH_Editor::Instance();

	while (!glfwWindowShouldClose(KH_Editor::Instance().GLFWwindow()))
	{
		Editor.BeginRender();

		Editor.BindCanvasFramebuffer();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Bunny.Render(TestShader);

		//GpuLBVHDebugBoxScene.Render();
		Editor.Render();

		Editor.UnbindCanvasFramebuffer();

		KH_PostProcessHelper::Instance().SingleGammaCorrectionGraph.Execute();

		Editor.EndRender();
	}

	return 0;
}
