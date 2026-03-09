#include "Editor/KH_Editor.h"
//#include "Pipeline/KH_Shader.h"
//#include "Scene/KH_Object.h"
//#include "Hit/KH_BVH.h"
//#include "Renderer/KH_Renderer.h"
//#include "Scene/KH_Scene.h"
//#include "Utils/KH_DebugUtils.h"

#include "Utils/KH_GPUAlgorithms.h"

int main()
{
	KH_Editor::EditorWidth = 1280;
	KH_Editor::EditorHeight = 920;
	KH_Editor::Title = "KH_Renderer";
	KH_Editor::Instance();

	std::vector<int> Data(6400000, 0);

	for (int i = 0; i < 6400000; i++)
		Data[i] = 6400000 - i;

	KH_RadixSort RadixSort;

	RadixSort.RunRadixSort(Data.data(), Data.size());

	return 0;
}


/*
int main()
{
	KH_Editor::EditorWidth = 1280;
	KH_Editor::EditorHeight = 920;
	KH_Editor::Title = "KH_Renderer";
	KH_Editor::Instance();

	KH_Model& Bunny = KH_DefaultModels::Instance().Bunny;
	KH_Scene& BunnyScene = KH_ExampleScenes::Instance().ExampleScene1;
	KH_Shader& TestShader = KH_ExampleShaders::Instance().TestShader;
	KH_Shader& AABBShader = KH_ExampleShaders::Instance().AABBShader;

	//KH_RendererBase BaseRenderer;
	//BaseRenderer.TraversalMode = KH_PRIMITIVE_TRAVERSAL_MODE::BASE_BVH;
	//BaseRenderer.Render(KH_ExampleScenes::Instance().ExampleScene1);

	KH_Framebuffer& Framebuffer = KH_Editor::Instance().GetCanvasFramebuffer();

	while (!glfwWindowShouldClose(KH_Editor::Instance().GLFWwindow()))
	{
		KH_Editor::Instance().BeginRender();

		Framebuffer.Bind();
		glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		Framebuffer.Unbind();

		//Bunny.Render(TestShader);
		//BunnyScene.BVH.RenderAABB(AABBShader, glm::vec3(1.0f, 1.0f, 1.0f));

		BunnyScene.Render();

		KH_Editor::Instance().EndRender();
	}

	return 0;
}
*/
