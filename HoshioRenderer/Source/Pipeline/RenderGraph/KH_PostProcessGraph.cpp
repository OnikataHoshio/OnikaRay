#include "KH_PostProcessGraph.h"

#include "Editor/KH_Editor.h"

#include "PostProcess/KH_GammaCorrectionPass.h"

void KH_PostProcessGraph::AddPass(std::unique_ptr<KH_PostProcessPass> pass)
{
	Passes.push_back(std::move(pass));
}

void KH_PostProcessGraph::Execute()
{
    bool firstPass = true;

    KH_Canvas& canvas = KH_Editor::Instance().GetCanvas();

    for (auto& pass : Passes)
    {
        if (!pass->IsEnabled())
            continue;

        KH_Framebuffer& input = firstPass ? canvas.GetSceneFramebuffer()
            : canvas.GetLastPostProcessFramebuffer();
        KH_Framebuffer& output = canvas.GetCurrentPostProcessFramebuffer();

        pass->Execute(input, output);

        canvas.SwapPostProcessFramebuffers();
        firstPass = false;
    }
}

KH_PostProcessHelper::KH_PostProcessHelper()
{
    InitPostProcessGraphs();
}

void KH_PostProcessHelper::InitPostProcessGraphs()
{
    InitSingleGammaCorrectionGraph();
}

void KH_PostProcessHelper::InitSingleGammaCorrectionGraph()
{
    KH_Shader& Shader = KH_ExampleShaders::Instance().GammaCorrectionShader;
    auto GammaCorrection = std::make_unique<KH_GammaCorrectionPass>("GammaCorrection", Shader, 2.2);

    SingleGammaCorrectionGraph.AddPass(std::move(GammaCorrection));
}

void KH_PostProcessHelper::InitDrawSobelGraph()
{

}
