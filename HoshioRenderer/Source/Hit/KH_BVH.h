#pragma once
#include "KH_AABB.h"
#include "Pipeline/KH_Buffer.h"

class KH_Shader;

class KH_Model;
class KH_Primitive;

using KH_SceneObject = std::unique_ptr<KH_Model>;
using KH_ScenePrimitive = std::unique_ptr<KH_Primitive>;

enum class KH_BVH_SPLIT_MODE
{
	X_AXIS_SPLIT = 0,
	Y_AXIS_SPLIT = 1,
	Z_AXIS_SPLIT = 2
};

enum class KH_BVH_BUILD_MODE
{
	Base = 0,
	SAH = 1
};

struct KH_BVHHitInfo
{
	bool bIsHit = false;
	float HitTime = std::numeric_limits<float>::max();
	uint32_t BeginIndex = 0;
	uint32_t EndIndex = 0;
};

struct KH_BVHSplitInfo
{
	KH_BVH_SPLIT_MODE SplitMode = KH_BVH_SPLIT_MODE::X_AXIS_SPLIT;
	float Cost = std::numeric_limits<float>::max();
	uint32_t SplitIndex = 0;
};

#pragma region IBVH
class KH_IBVHNode
{
public:
	KH_IBVHNode() = default;
	virtual ~KH_IBVHNode() = default;

	bool bIsLeaf = false;
	int Offset = 0;
	int Size = 0;

	KH_AABB AABB;

protected:
	static KH_BVH_SPLIT_MODE SelectSplitMode(KH_AABB& AABB);

	static KH_BVHSplitInfo SelectSplitModeSAH(std::vector<KH_ScenePrimitive>& Primitives, int BeginIndex, int EndIndex);
};

class KH_IBVH
{
public:
	KH_IBVH() = default;
	KH_IBVH(uint32_t MaxBVHDepth, uint32_t MaxLeafPrimitives, KH_BVH_BUILD_MODE BuildMode = KH_BVH_BUILD_MODE::Base);
	virtual ~KH_IBVH() = default;

	std::vector<KH_ScenePrimitive> Primitives;
	uint32_t PrimitiveCount = 0;

	uint32_t MaxBVHDepth = 8;
	uint32_t MaxLeafPrimitives = 8;

	std::vector<glm::mat4> ModelMats;
	uint32_t MatCount = 0;

	KH_BVH_BUILD_MODE BuildMode = KH_BVH_BUILD_MODE::Base;

	KH_SSBO<glm::mat4> ModelMats_SSBO;

	static constexpr bool bIsBuildOnCPU = true;

	void RenderAABB(KH_Shader& Shader, glm::vec3 Color) const;

	virtual void BindAndBuild(std::vector<KH_SceneObject>& Objects) = 0;

	virtual std::vector<KH_BVHHitInfo> Hit(KH_Ray& Ray) = 0;

protected:
	virtual void FillModelMatrices(uint32_t TargetDepth) = 0;

	void CollectPrimitives(std::vector<KH_SceneObject>& Objects);

	void UpdateModelMatsSSBO();

	virtual void BuildBVH() = 0;

};

#pragma endregion

#pragma region BVH

class KH_BVHNode : public KH_IBVHNode
{
public:
	std::unique_ptr<KH_BVHNode> Left;
	std::unique_ptr<KH_BVHNode> Right;

	void BuildNode(std::vector<KH_ScenePrimitive>& Primitives, uint32_t BeginIndex, uint32_t EndIndex, uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth);

	void BuildNodeSAH(std::vector<KH_ScenePrimitive>& Primitives, uint32_t BeginIndex, uint32_t EndIndex, uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth);

	void Hit(std::vector<KH_BVHHitInfo>& HitInfos, KH_Ray& Ray);
};

class KH_BVH : public KH_IBVH
{
public:
	std::unique_ptr<KH_BVHNode> Root;

	KH_BVH();
	KH_BVH(uint32_t MaxBVHDepth, uint32_t MaxLeafPrimitives, KH_BVH_BUILD_MODE BuildMode = KH_BVH_BUILD_MODE::Base);
	~KH_BVH() override = default;

	void BindAndBuild(std::vector<KH_SceneObject>& Objects) override;

	std::vector<KH_BVHHitInfo> Hit(KH_Ray& Ray) override;

private:
	void FillModelMatrices(uint32_t TargetDepth) override;

	void FillModelMatrices_Inner(KH_BVHNode* BVHNode, uint32_t CurrentDepth, uint32_t TargetDepth);

	void BuildBVH() override;
};

#pragma endregion

#pragma region FlatBVH

class KH_FlatBVHNode : public KH_IBVHNode
{
public:
	int Left, Right;

	static int BuildNode(std::vector<KH_ScenePrimitive>& Primitives, std::vector<KH_FlatBVHNode>& FlatBVHNodes, uint32_t BeginIndex, uint32_t EndIndex, uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth);

	static int BuildNodeSAH(std::vector<KH_ScenePrimitive>& Primitives, std::vector<KH_FlatBVHNode>& FlatBVHNodes, uint32_t BeginIndex, uint32_t EndIndex, uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth);

	void Hit(std::vector<KH_BVHHitInfo>& HitInfos, std::vector<KH_FlatBVHNode>& FlatBVHNodes, KH_Ray& Ray);
};

#define KH_FLAT_BVH_NULL_NODE -1

class KH_FlatBVH : public KH_IBVH
{
public:
	int Root = KH_FLAT_BVH_NULL_NODE;
	std::vector<KH_FlatBVHNode> BVHNodes;

	KH_FlatBVH() = default;
	KH_FlatBVH(uint32_t MaxBVHDepth, uint32_t MaxLeafPrimitives, KH_BVH_BUILD_MODE BuildMode = KH_BVH_BUILD_MODE::Base);
	~KH_FlatBVH() override = default;

	void BindAndBuild(std::vector<KH_SceneObject>& Objects) override;
	std::vector<KH_BVHHitInfo> Hit(KH_Ray& Ray) override;

private:
	void FillModelMatrices(uint32_t TargetDepth) override;

	void FillModelMatrices_Inner(int FlatBVHNodeID, uint32_t CurrentDepth, uint32_t TargetDepth);

	void BuildBVH() override;
};

#pragma endregion
