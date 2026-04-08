#pragma once

#include "KH_Mesh.h"
#include "Pipeline/KH_Texture.h"

enum class KH_ModelSourceType
{
    Asset,
    Builtin,
    Inline
};

enum class KH_BuiltinModelType
{
    None,
    FullscreenQuad,
    Plane,
    Cube,
    EmptyCube
};


class KH_Model : public KH_Hitable
{
public:
    KH_Model() = default;
    virtual ~KH_Model() override = default;
    KH_Model(const std::string& path);
    KH_Model(const KH_Model&) = delete;
    KH_Model& operator=(const KH_Model&) = delete;
    KH_Model(KH_Model&& other) noexcept;
    KH_Model& operator=(KH_Model&& other) noexcept;

    void LoadModel(const std::string& path);

    static KH_Model CreateBuiltin(KH_BuiltinModelType type, float size = 1.0f);

    KH_HitResult Hit(const KH_Ray& Ray) const override;
    virtual KH_PickResult Pick(const KH_Ray& Ray) const;

    void AddMesh(KH_Mesh&& mesh);
    virtual uint32_t GetPrimitiveCount() const override;
    virtual void EncodePrimitives(std::vector<KH_PrimitiveEncoded>& outPrimitives) const override;
    virtual void CollectPrimitives(std::vector<KH_ScenePrimitive>& outPrimitives) const override;
    virtual void CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const override;
    virtual const KH_AABB& GetAABB() const override;

    KH_ModelSourceType GetSourceType() const { return SourceType; }
    KH_BuiltinModelType GetBuiltinType() const { return BuiltinType; }
    float GetBuiltinSize() const { return BuiltinSize; }
    const std::string& GetSourcePath() const { return SourcePath; }
    std::vector<KH_Mesh>& GetMeshes() { return Meshes; }

    void SetSourceAsInline();
    void SetMeshMaterialSlotID(int MaterialSlotID = 0);
    void SetMeshMaterialSlotID(int MaterialSlotID, int MeshID);

    void Render(KH_Shader& Shader);

private:
    std::vector<KH_Mesh> Meshes;
    std::string Directory;
    std::string SourcePath;

    KH_ModelSourceType SourceType = KH_ModelSourceType::Inline;
    KH_BuiltinModelType BuiltinType = KH_BuiltinModelType::None;
    float BuiltinSize = 1.0f;

    void ProcessNode(aiNode* node, const aiScene* scene);
    KH_Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<KH_Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type);

    void UpdateAABB() override;
    void UpdateGizmoPivotLocal();
    void OnTransformChanged() override;

};

class KH_PrimitiveFactory
{
public:
    static KH_Mesh CreateFullscreenQuadMesh(float size = 1.0f);
    static KH_Mesh CreatePlaneMesh(float size = 1.0f);
    static KH_Mesh CreateCubeMesh(float size = 1.0f);
    static KH_Mesh CreateEmptyCubeMesh(float size = 1.0f);
    static KH_Model CreateFullscreenQuad(float size = 1.0f);
    static KH_Model CreatePlane(float size = 1.0f);
    static KH_Model CreateCube(float size = 1.0f);
    static KH_Model CreateEmptyCube(float size = 1.0f);
};

class KH_DefaultModels : public KH_Singleton<KH_DefaultModels>
{
	friend class KH_Singleton<KH_DefaultModels>;
public:
	KH_Mesh Cube;
	KH_Mesh EmptyCube;
	KH_Mesh FullscreenQuad;
    KH_Model Plane;
	KH_Model Bunny;

	KH_DefaultModels(const KH_DefaultModels&) = delete;
	KH_DefaultModels& operator=(const KH_DefaultModels&) = delete;

private:
	KH_DefaultModels();
	~KH_DefaultModels() override = default;

	void InitCube();

	void InitEmptyCube();

	void InitFullscreenQuad();

    void InitPlane();

	void InitBunny();

};