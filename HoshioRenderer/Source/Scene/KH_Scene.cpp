#include "KH_Scene.h"
#include "Editor/KH_Editor.h"
#include "Pipeline/KH_Shader.h"
#include "Pipeline/KH_Texture.h"
#include "KH_SceneXmlSerializer.h"

void KH_SceneBase::SetCameraParamUBO()
{
    KH_CameraParam CameraParam;
    const KH_Camera& Camera = KH_Editor::Instance().Camera;
    CameraParam.AspectAndFovy = glm::vec4(Camera.Aspect, Camera.Fovy, 0.0f, 0.0f);
    CameraParam.Position = glm::vec4(Camera.Position, 1.0f);
    CameraParam.Right = glm::vec4(Camera.Right, 1.0f);
    CameraParam.Up = glm::vec4(Camera.Up, 1.0f);
    CameraParam.Front = glm::vec4(Camera.Front, 1.0f);

    CameraParam_UB0.SetSingleData(CameraParam);
}

void KH_SceneBase::SetAndBindCameraParamUB0()
{
    SetCameraParamUBO();
    CameraParam_UB0.Bind();
}

std::vector<KH_PrimitiveEncoded> KH_SceneBase::EncodePrimitives()
{
    std::vector<KH_PrimitiveEncoded> Encoded;

    PrimitiveCount = 0;
    for (int i = 0; i < Objects.size(); i++)
    {
        PrimitiveCount += Objects[i]->GetPrimitiveCount();
    }

    Encoded.reserve(PrimitiveCount);

    for (const auto& sceneObject : Objects)
    {
        sceneObject->EncodePrimitives(Encoded);
    }

    return Encoded;
}

std::vector<KH_BRDFMaterialEncoded> KH_SceneBase::EncodeBRDFMaterials() const
{
    const int nMaterials = Materials.size();
    std::vector<KH_BRDFMaterialEncoded> BSDFMaterialEncodeds(nMaterials);

    for (int i = 0; i < nMaterials; i++)
    {
        const KH_BRDFMaterial& Mat = Materials[i];

        BSDFMaterialEncodeds[i].Emissive = glm::vec4(Mat.Emissive, 1.0);
        BSDFMaterialEncodeds[i].BaseColor = glm::vec4(Mat.BaseColor, 1.0);
        BSDFMaterialEncodeds[i].Param1 = glm::vec4(Mat.Subsurface, Mat.Metallic, Mat.Specular, Mat.SpecularTint);
        BSDFMaterialEncodeds[i].Param2 = glm::vec4(Mat.Roughness, Mat.Anisotropic, Mat.Sheen, Mat.SheenTint);
        BSDFMaterialEncodeds[i].Param3 = glm::vec4(Mat.Clearcoat, Mat.ClearcoatGloss, Mat.IOR, Mat.Transmission);
    }

    return BSDFMaterialEncodeds;
}

std::vector<KH_SceneObject>& KH_SceneBase::GetObjects()
{
    return Objects;
}

const std::vector<KH_SceneObject>& KH_SceneBase::GetObjects() const
{
    return Objects;
}

KH_Model& KH_SceneBase::AddModel(int MaterialSlotID, const std::string& Path)
{
    auto obj = std::make_unique<KH_Model>(Path);
    KH_Model& ref = *obj;

    if (MaterialSlotID != KH_MATERIAL_UNDEFINED_SLOT)
    {
        ref.SetMeshMaterialSlotID(MaterialSlotID);
    }

    Objects.push_back(std::move(obj));
    return ref;
}

KH_Model& KH_SceneBase::AddModel(int MaterialSlotID, KH_Model&& Model)
{
    auto obj = std::make_unique<KH_Model>(std::move(Model));
    KH_Model& ref = *obj;

    if (MaterialSlotID != KH_MATERIAL_UNDEFINED_SLOT)
    {
        ref.SetMeshMaterialSlotID(MaterialSlotID);
    }

    Objects.push_back(std::move(obj));
    return ref;
}

KH_Model& KH_SceneBase::AddEmptyModel(int MaterialSlotID)
{
    auto obj = std::make_unique<KH_Model>();
    KH_Model& ref = *obj;

    if (MaterialSlotID != KH_MATERIAL_UNDEFINED_SLOT)
    {
        ref.SetMeshMaterialSlotID(MaterialSlotID);
    }

    Objects.push_back(std::move(obj));
    return ref;
}

// 新增
int KH_SceneBase::AddMaterial(const KH_BRDFMaterial& material)
{
    Materials.push_back(material);
    return static_cast<int>(Materials.size()) - 1;
}

// 新增
bool KH_SceneBase::DeleteMaterial(int materialID)
{
    if (materialID < 0 || materialID >= static_cast<int>(Materials.size()))
        return false;

    Materials.erase(Materials.begin() + materialID);

    const int fallbackID = Materials.empty() ? KH_MATERIAL_UNDEFINED_SLOT : 0;

    for (auto& sceneObject : Objects)
    {
        for (auto& mesh : sceneObject->GetMeshes())
        {
            int MeshMaterialSlotID = mesh.GetMaterialSlotID();
            if (MeshMaterialSlotID == materialID)
            {
                mesh.SetMaterialSlotID(materialID);
            }
            else if (MeshMaterialSlotID > materialID)
            {
                mesh.SetMaterialSlotID(MeshMaterialSlotID - 1);
            }
	        
        }
    }

    return true;
}

bool KH_SceneBase::RemoveObjectAt(size_t Index)
{
    if (Index >= Objects.size())
        return false;

    Objects.erase(Objects.begin() + static_cast<std::ptrdiff_t>(Index));
    return true;
}

void KH_SceneBase::Clear()
{
    Objects.clear();
    AABB.Reset();
}

bool KH_SceneBase::SaveToXml(const std::string& filePath) const
{
    return KH_SceneXmlSerializer::SaveScene(*this, filePath);
}

bool KH_SceneBase::LoadFromXml(const std::string& filePath)
{
    return KH_SceneXmlSerializer::LoadScene(*this, filePath);
}

KH_PickResult KH_SceneBase::Pick(const KH_Ray& ray) const
{
    KH_PickResult best;

    for (int i = 0; i < static_cast<int>(Objects.size()); ++i)
    {
        const KH_SceneObject& sceneObject = Objects[i];
        if (!sceneObject)
            continue;

        if (!sceneObject->GetAABB().Hit(ray).bIsHit)
            continue;

        KH_PickResult pick = sceneObject->Pick(ray);

        if (pick.bIsHit && pick.Distance < best.Distance)
        {
            best = pick;
            best.ObjectIndex = i;   // 修这里，不是 ObjectMeshID
        }
    }

    return best;
}

void KH_GpuLBVHScene::SetSSBOs()
{
    std::vector<KH_PrimitiveEncoded> PrimitiveEncodeds = EncodePrimitives();
    std::vector<KH_BRDFMaterialEncoded> BSDFMaterialEncodeds = EncodeBRDFMaterials();

    Primitive_SSBO.SetData(PrimitiveEncodeds);
    Material_SSBO.SetData(BSDFMaterialEncodeds);
}

void KH_GpuLBVHScene::SetRayTracingParam(KH_Shader& Shader)
{
    Shader.Use();

    const KH_Camera& Camera = KH_Editor::Instance().Camera;

    Primitive_SSBO.Bind();
    BVH.Morton3DSSBO.Bind();
    BVH.LBVHNodeSSBO.Bind();
    BVH.AuxiliarySSBO.Bind();
    Material_SSBO.Bind();

    Shader.SetInt("uLBVHNodeCount", BVH.LBVHNodeCount);
    Shader.SetUint("uFrameCounter", KH_Editor::Instance().GetFrameCounter());
    Shader.SetUvec2("uResolution", glm::uvec2(KH_Editor::GetCanvasWidth(), KH_Editor::GetCanvasHeight()));

    KH_Editor::Instance().GetLastFramebuffer().BindColorAttachment(0, 0);
    KH_ExampleTextures::Instance().FirePlaceHDR.Bind(1);

    Shader.SetInt("uLastFrame", 0);
    Shader.SetInt("uSkybox", 1);

    SetAndBindCameraParamUB0();
}

void KH_GpuLBVHScene::UpdateAABB()
{
    AABB.Reset();
    for (auto& Object :Objects)
    {
        AABB.Merge(Object->GetAABB());
    }
}

void KH_GpuLBVHScene::BindAndBuild()
{
    SetSSBOs();
    UpdateAABB();
    BVH.BindAndBuild(this);
}

void KH_GpuLBVHScene::UpdateMaterialSSBO()
{
    std::vector<KH_BRDFMaterialEncoded> BSDFMaterialEncodeds = EncodeBRDFMaterials();
    Material_SSBO.SetData(BSDFMaterialEncodeds);
}

void KH_GpuLBVHScene::UpdatePrimitiveSSBO()
{
    std::vector<KH_PrimitiveEncoded> PrimitiveEncodeds = EncodePrimitives();
    Primitive_SSBO.SetData(PrimitiveEncodeds);
}

void KH_GpuLBVHScene::Render()
{
    SetRayTracingParam(KH_ExampleShaders::Instance().DisneyBRDF_0);

    KH_Editor::Instance().BindCanvasFramebuffer();

    glBindVertexArray(KH_DefaultModels::Instance().FullscreenQuad.GetVAO());
    glDrawElements(GL_TRIANGLES, KH_DefaultModels::Instance().FullscreenQuad.GetNumIndices(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    KH_Editor::Instance().UnbindCanvasFramebuffer();
}

