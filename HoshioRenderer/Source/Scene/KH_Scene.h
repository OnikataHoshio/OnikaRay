#pragma once
#include "Hit/KH_BVH.h"
#include "KH_Object.h"

enum class KH_BVH_BUILD_MODE;

struct KH_TriangleEncoded
{
	glm::vec4 P1, P2, P3;
	glm::vec4 N1, N2, N3;
	glm::ivec4 MaterialSlot;
};

struct KH_BRDFMaterialEncoded
{
	glm::vec4 Emissive;
	glm::vec4 BaseColor;
	glm::vec4 Param1;
	glm::vec4 Param2;
	glm::vec4 Param3;
};


struct KH_LBVHNodeEncoded
{
	glm::ivec4 Param1; //(Left, Right, )
	glm::ivec4 Param2; //(bIsLeaf, Offset, Size)
	glm::vec4 AABB_MinPos;
	glm::vec4 AABB_MaxPos;
};

class KH_Scene
{
private:
	void SetSSBOs();

	KH_SSBO<KH_TriangleEncoded> Triangle_SSBO;
	KH_SSBO<KH_BRDFMaterialEncoded> Material_SSBO;
	KH_SSBO<KH_LBVHNodeEncoded> LBVHNode_SSBO;

	void SetRayTracingParam() const;

public:
	KH_Scene();
	KH_Scene(uint32_t MaxBVHDepth, uint32_t MaxLeafTrianglesm, KH_BVH_BUILD_MODE BuildMode);
	~KH_Scene() = default;

	std::vector<KH_Model> Models;
	std::vector<KH_Triangle> Triangles;
	std::vector<KH_BRDFMaterial> Materials;

	KH_LBVH BVH;
	//KH_BVH BVH;

	void LoadObj(const std::string& path, float scale = 1.0, int MaterialSlot = KH_MATERIAL_UNDEFINED_SLOT);

	void AddTriangles(KH_Triangle& Triangle);

	void BindAndBuild();

	std::vector<KH_TriangleEncoded> EncodeTriangles();

	std::vector<KH_BRDFMaterialEncoded> EncodeBSDFMaterials();

	std::vector<KH_LBVHNodeEncoded> EncodeLBVHNodes();

	void Render();

	void Clear();
};


class KH_ExampleScenes : public KH_Singleton<KH_ExampleScenes>
{
	friend class KH_Singleton<KH_ExampleScenes>;

private:
	KH_ExampleScenes();
	virtual ~KH_ExampleScenes() override = default;

	void InitExampleScene1();

	void InitSingleTriangle();
public:
	KH_Scene SingleTriangle;

	KH_Scene ExampleScene1;


};



