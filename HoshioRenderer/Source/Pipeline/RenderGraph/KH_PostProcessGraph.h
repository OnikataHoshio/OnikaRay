#pragma once

#include "KH_PostProcessPass.h"

class KH_Canvas;

class KH_PostProcessGraph
{
public:
    void AddPass(std::unique_ptr<KH_PostProcessPass> pass);

    void Execute();

private:
    std::vector<std::unique_ptr<KH_PostProcessPass>> Passes;
};


class KH_PostProcessHelper : public KH_Singleton<KH_PostProcessHelper>
{
    friend class KH_Singleton<KH_PostProcessHelper>;
private:
    KH_PostProcessHelper();
    virtual ~KH_PostProcessHelper() override = default;

    void InitPostProcessGraphs();

    void InitSingleGammaCorrectionGraph();

    void InitDrawSobelGraph();

public:
    KH_PostProcessGraph SingleGammaCorrectionGraph;

    KH_PostProcessGraph DrawSobelGraph;


};

