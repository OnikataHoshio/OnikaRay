#include "KH_BVH.h"
#include "KH_AABB.h"
#include "Editor/KH_Editor.h"
#include "Scene/KH_Model.h"
#include "Pipeline/KH_Shader.h"

#include "Utils/KH_DebugUtils.h"


void KH_IBVH::RenderAABB(KH_Shader& Shader, glm::vec3 Color) const
{
	KH_Editor::Instance().BindCanvasFramebuffer();

	Shader.Use();
	Shader.SetMat4("view", KH_Editor::Instance().Camera.GetViewMatrix());
	Shader.SetMat4("projection", KH_Editor::Instance().Camera.GetProjMatrix());
	Shader.SetVec3("Color", Color);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glDisable(GL_CULL_FACE);

	ModelMats_SSBO.Bind();

	glBindVertexArray(KH_DefaultModels::Instance().EmptyCube.GetVAO());
	glDrawElementsInstanced(
	    GL_LINES,
		KH_DefaultModels::Instance().Cube.GetNumIndices(),
		GL_UNSIGNED_INT,
		0,
		MatCount	);
	glBindVertexArray(0);

	ModelMats_SSBO.Unbind();
	KH_Editor::Instance().UnbindCanvasFramebuffer();
}


void KH_IBVH::CollectPrimitives(std::vector<KH_SceneObject>& Objects)
{
	PrimitiveCount = 0;
	for (auto& Object : Objects)
	{
		PrimitiveCount += Object->GetPrimitiveCount();
	}
	Primitives.reserve(PrimitiveCount);
	for (auto& Object : Objects)
	{
		Object->CollectPrimitives(Primitives);
	}
}

void KH_IBVH::UpdateModelMatsSSBO()
{
	ModelMats_SSBO.SetData(ModelMats, GL_STATIC_DRAW);
}


KH_BVH_SPLIT_MODE KH_IBVHNode::SelectSplitMode(KH_AABB& AABB)
{
	glm::vec3 Size = AABB.GetSize();
	int axis = 0;
	if (Size.y > Size[axis]) axis = 1;
	if (Size.z > Size[axis]) axis = 2;

	return static_cast<KH_BVH_SPLIT_MODE>(axis);
}

KH_BVHSplitInfo KH_IBVHNode::SelectSplitModeSAH(std::vector<KH_ScenePrimitive>& Primitives, int BeginIndex,
	int EndIndex)
{
	int count = EndIndex - BeginIndex;
	KH_BVHSplitInfo BestSplit;
	BestSplit.Cost = std::numeric_limits<float>::max();

	std::vector<float> leftAreas(count);
	std::vector<KH_AABB> leftBoxes(count);

	for (int axis = 0; axis < 3; axis++)
	{
		auto comparator = (axis == 0) ? KH_Primitive::CmpxPtr : (axis == 1) ? KH_Primitive::CmpyPtr : KH_Primitive::CmpzPtr;
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, comparator);

		KH_AABB currentLeft;
		for (int i = 0; i < count; ++i) {
			currentLeft.Merge(Primitives[BeginIndex + i]->GetAABB());
			leftAreas[i] = currentLeft.GetSurfaceArea();
			leftBoxes[i] = currentLeft; // 可选：用于调试
		}

		KH_AABB currentRight;
		for (int i = count - 1; i > 0; --i) {
			currentRight.Merge(Primitives[BeginIndex + i]->GetAABB());

			float saLeft = leftAreas[i - 1];
			float saRight = currentRight.GetSurfaceArea();
			int nLeft = i;
			int nRight = count - i;

			float totalCost = (float)nLeft * saLeft + (float)nRight * saRight;

			if (totalCost < BestSplit.Cost) {
				BestSplit.Cost = totalCost;
				BestSplit.SplitIndex = BeginIndex + i; 
				BestSplit.SplitMode = static_cast<KH_BVH_SPLIT_MODE>(axis);
			}
		}
	}
	return BestSplit;
}

KH_IBVH::KH_IBVH(uint32_t MaxBVHDepth, uint32_t MaxLeafPrimitives, KH_BVH_BUILD_MODE BuildMode)
	:MaxBVHDepth(MaxBVHDepth), MaxLeafPrimitives(MaxLeafPrimitives), BuildMode(BuildMode)
{
}

void KH_BVHNode::BuildNode(std::vector<KH_ScenePrimitive>& Primitives, uint32_t BeginIndex, uint32_t EndIndex, uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth)
{
	int count = EndIndex - BeginIndex;
	if (count <= 0) return;

	float MaxInf = std::numeric_limits<float>::max();
	AABB.MinPos = glm::vec3(MaxInf);
	AABB.MaxPos = glm::vec3(-MaxInf);

	for (int i = BeginIndex; i < EndIndex; i++) {
		AABB.MinPos = glm::min(AABB.MinPos, Primitives[i]->GetAABB().MinPos);
		AABB.MaxPos = glm::max(AABB.MaxPos, Primitives[i]->GetAABB().MaxPos);
	}

	AABB.MinPos -= static_cast<float>(EPS);
	AABB.MaxPos += static_cast<float>(EPS);

	if (count <= MaxNum || Depth >= MaxDepth) {

		this->bIsLeaf = true;
		this->Offset = BeginIndex;
		this->Size = count;
		this->Left = nullptr;
		this->Right = nullptr;
		return;
	}

	KH_BVH_SPLIT_MODE SplitMode = SelectSplitMode(AABB);
	switch (SplitMode) {
	case KH_BVH_SPLIT_MODE::X_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpxPtr);
		break;
	case KH_BVH_SPLIT_MODE::Y_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpyPtr);
		break;
	case KH_BVH_SPLIT_MODE::Z_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpzPtr);
		break;
	}

	int Mid = BeginIndex + count / 2;

	this->bIsLeaf = false;

	this->Left = std::make_unique<KH_BVHNode>();
	this->Left->BuildNode(Primitives, BeginIndex, Mid, Depth + 1, MaxNum, MaxDepth);

	this->Right = std::make_unique<KH_BVHNode>();
	this->Right->BuildNode(Primitives, Mid, EndIndex, Depth + 1, MaxNum, MaxDepth);
}

void KH_BVHNode::BuildNodeSAH(std::vector<KH_ScenePrimitive>& Primitives, uint32_t BeginIndex, uint32_t EndIndex,
	uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth)
{
	int count = EndIndex - BeginIndex;
	if (count <= 0) return;

	float MaxInf = std::numeric_limits<float>::max();
	AABB.MinPos = glm::vec3(MaxInf);
	AABB.MaxPos = glm::vec3(-MaxInf);

	for (int i = BeginIndex; i < EndIndex; i++) {
		AABB.MinPos = glm::min(AABB.MinPos, Primitives[i]->GetAABB().MinPos);
		AABB.MaxPos = glm::max(AABB.MaxPos, Primitives[i]->GetAABB().MaxPos);
	}

	AABB.MinPos -= static_cast<float>(EPS);
	AABB.MaxPos += static_cast<float>(EPS);

	if (count <= MaxNum || Depth >= MaxDepth) {
		this->bIsLeaf = true;
		this->Offset = BeginIndex;
		this->Size = count;
		this->Left = nullptr;
		this->Right = nullptr;
		return;
	}

	KH_BVHSplitInfo SplitInfo = SelectSplitModeSAH(Primitives, BeginIndex, EndIndex);
	switch (SplitInfo.SplitMode) {
	case KH_BVH_SPLIT_MODE::X_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpxPtr);
		break;
	case KH_BVH_SPLIT_MODE::Y_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpyPtr);
		break;
	case KH_BVH_SPLIT_MODE::Z_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpzPtr);
		break;
	}

	this->bIsLeaf = false;

	this->Left = std::make_unique<KH_BVHNode>();
	this->Left->BuildNodeSAH(Primitives, BeginIndex, SplitInfo.SplitIndex, Depth + 1, MaxNum, MaxDepth);

	this->Right = std::make_unique<KH_BVHNode>();
	this->Right->BuildNodeSAH(Primitives, SplitInfo.SplitIndex, EndIndex, Depth + 1, MaxNum, MaxDepth);
}

void KH_BVHNode::Hit(std::vector<KH_BVHHitInfo>& HitInfos, KH_Ray& Ray)
{
	KH_AABBHitInfo AABBHit = AABB.Hit(Ray);
	if (!AABBHit.bIsHit) return;

	if (bIsLeaf)
	{
		KH_BVHHitInfo BVHHitInfo;
		BVHHitInfo.BeginIndex = Offset;
		BVHHitInfo.EndIndex = Offset + Size;
		BVHHitInfo.HitTime = AABBHit.HitTime; 
		BVHHitInfo.bIsHit = true;
		HitInfos.push_back(BVHHitInfo);
		return;
	}

	if (Left)  Left->Hit(HitInfos, Ray);
	if (Right) Right->Hit(HitInfos, Ray);
}

KH_BVH::KH_BVH()
{
	Root = std::make_unique<KH_BVHNode>();
}

KH_BVH::KH_BVH(uint32_t MaxBVHDepth, uint32_t MaxLeafPrimitives, KH_BVH_BUILD_MODE BuildMode)
	:KH_IBVH(MaxBVHDepth, MaxLeafPrimitives,BuildMode)
{
	Root = std::make_unique<KH_BVHNode>();
}

void KH_BVH::BindAndBuild(std::vector<KH_SceneObject>& Objects)
{
	CollectPrimitives(Objects);

	BuildBVH();
	FillModelMatrices(MaxBVHDepth);

	//std::string DebugMessage = std::format("Model Range : [({},{},{}),({},{},{})]",
	//	Root->AABB.MinPos.x, Root->AABB.MinPos.y, Root->AABB.MinPos.z,
	//	Root->AABB.MaxPos.x, Root->AABB.MaxPos.y, Root->AABB.MaxPos.z);
	//LOG_D(DebugMessage);
}

std::vector<KH_BVHHitInfo> KH_BVH::Hit(KH_Ray& Ray)
{
	std::vector<KH_BVHHitInfo> HitInfos;
	if (Root != nullptr)
		Root->Hit(HitInfos, Ray);
	return HitInfos;
}


void KH_BVH::FillModelMatrices(uint32_t TargetDepth)
{
	ModelMats.clear();
	MatCount = 0;

	FillModelMatrices_Inner(Root.get(), 0, TargetDepth);

	UpdateModelMatsSSBO();
}

void KH_BVH::FillModelMatrices_Inner(KH_BVHNode* BVHNode, uint32_t CurrentDepth, uint32_t TargetDepth)
{
	if (BVHNode == nullptr)
		return;

	if (CurrentDepth == TargetDepth || BVHNode->bIsLeaf)
	{
		//TODO : Do some optimization
		ModelMats.push_back(BVHNode->AABB.GetModelMatrix());
		MatCount += 1;
		return;
	}

	FillModelMatrices_Inner(BVHNode->Left.get(), CurrentDepth + 1, TargetDepth);
	FillModelMatrices_Inner(BVHNode->Right.get(), CurrentDepth + 1, TargetDepth);
}

void KH_BVH::BuildBVH()
{
	switch (BuildMode)
	{
	case KH_BVH_BUILD_MODE::Base:
		this->Root->BuildNode(Primitives, 0, PrimitiveCount, 0, MaxLeafPrimitives, MaxBVHDepth);
		break;
	case KH_BVH_BUILD_MODE::SAH:
		this->Root->BuildNodeSAH(Primitives, 0, PrimitiveCount, 0, MaxLeafPrimitives, MaxBVHDepth);
		break;
	}
}

int KH_FlatBVHNode::BuildNode(std::vector<KH_ScenePrimitive>& Primitives, std::vector<KH_FlatBVHNode>& FlatBVHNodes,
	uint32_t BeginIndex, uint32_t EndIndex, uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth)
{
	int count = EndIndex - BeginIndex;
	if (count <= 0) return KH_FLAT_BVH_NULL_NODE;

	FlatBVHNodes.push_back({});

	int ID = FlatBVHNodes.size() - 1;

	float MaxInf = std::numeric_limits<float>::max();
	FlatBVHNodes[ID].AABB.MinPos = glm::vec3(MaxInf);
	FlatBVHNodes[ID].AABB.MaxPos = glm::vec3(-MaxInf);

	for (int i = BeginIndex; i < EndIndex; i++) {
		FlatBVHNodes[ID].AABB.MinPos = glm::min(FlatBVHNodes[ID].AABB.MinPos, Primitives[i]->GetAABB().MinPos);
		FlatBVHNodes[ID].AABB.MaxPos = glm::max(FlatBVHNodes[ID].AABB.MaxPos, Primitives[i]->GetAABB().MaxPos);
	}

	FlatBVHNodes[ID].AABB.MinPos -= static_cast<float>(EPS);
	FlatBVHNodes[ID].AABB.MaxPos += static_cast<float>(EPS);

	if (count <= MaxNum || Depth >= MaxDepth) {
		FlatBVHNodes[ID].bIsLeaf = true;
		FlatBVHNodes[ID].Offset = BeginIndex;
		FlatBVHNodes[ID].Size = count;
		FlatBVHNodes[ID].Left = KH_FLAT_BVH_NULL_NODE;
		FlatBVHNodes[ID].Right = KH_FLAT_BVH_NULL_NODE;
		return ID;
	}

	KH_BVH_SPLIT_MODE SplitMode = SelectSplitMode(FlatBVHNodes[ID].AABB);
	switch (SplitMode) {
	case KH_BVH_SPLIT_MODE::X_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpxPtr);
		break;
	case KH_BVH_SPLIT_MODE::Y_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpyPtr);
		break;
	case KH_BVH_SPLIT_MODE::Z_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpzPtr);
		break;
	}

	int MID = BeginIndex + count / 2;

	FlatBVHNodes[ID].bIsLeaf = false;

	int leftID = BuildNode(Primitives, FlatBVHNodes, BeginIndex, MID, Depth + 1, MaxNum, MaxDepth);
	FlatBVHNodes[ID].Left = leftID;

	int rightID = BuildNode(Primitives, FlatBVHNodes, MID, EndIndex, Depth + 1, MaxNum, MaxDepth);
	FlatBVHNodes[ID].Right = rightID;

	return ID;
}

int KH_FlatBVHNode::BuildNodeSAH(std::vector<KH_ScenePrimitive>& Primitives, std::vector<KH_FlatBVHNode>& FlatBVHNodes,
	uint32_t BeginIndex, uint32_t EndIndex, uint32_t Depth, uint32_t MaxNum, uint32_t MaxDepth)
{
	int count = EndIndex - BeginIndex;
	if (count <= 0) return KH_FLAT_BVH_NULL_NODE;

	FlatBVHNodes.push_back({});
	int ID = FlatBVHNodes.size() - 1;

	float MaxInf = std::numeric_limits<float>::max();
	FlatBVHNodes[ID].AABB.MinPos = glm::vec3(MaxInf);
	FlatBVHNodes[ID].AABB.MaxPos = glm::vec3(-MaxInf);

	for (int i = BeginIndex; i < EndIndex; i++) {
		FlatBVHNodes[ID].AABB.MinPos = glm::min(FlatBVHNodes[ID].AABB.MinPos, Primitives[i]->GetAABB().MinPos);
		FlatBVHNodes[ID].AABB.MaxPos = glm::max(FlatBVHNodes[ID].AABB.MaxPos, Primitives[i]->GetAABB().MaxPos);
	}

	FlatBVHNodes[ID].AABB.MinPos -= static_cast<float>(EPS);
	FlatBVHNodes[ID].AABB.MaxPos += static_cast<float>(EPS);

	if (count <= MaxNum || Depth >= MaxDepth) {
		FlatBVHNodes[ID].bIsLeaf = true;
		FlatBVHNodes[ID].Offset = BeginIndex;
		FlatBVHNodes[ID].Size = count;
		FlatBVHNodes[ID].Left = KH_FLAT_BVH_NULL_NODE;
		FlatBVHNodes[ID].Right = KH_FLAT_BVH_NULL_NODE;
		return ID;
	}

	KH_BVHSplitInfo SplitInfo = SelectSplitModeSAH(Primitives, BeginIndex, EndIndex);
	switch (SplitInfo.SplitMode) {
	case KH_BVH_SPLIT_MODE::X_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpxPtr);
		break;
	case KH_BVH_SPLIT_MODE::Y_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpyPtr);
		break;
	case KH_BVH_SPLIT_MODE::Z_AXIS_SPLIT:
		std::sort(Primitives.begin() + BeginIndex, Primitives.begin() + EndIndex, KH_Primitive::CmpzPtr);
		break;
	}

	FlatBVHNodes[ID].bIsLeaf = false;

	int leftID = BuildNodeSAH(Primitives, FlatBVHNodes, BeginIndex, SplitInfo.SplitIndex, Depth + 1, MaxNum, MaxDepth);
	FlatBVHNodes[ID].Left = leftID;

	int rightID = BuildNodeSAH(Primitives, FlatBVHNodes, SplitInfo.SplitIndex, EndIndex, Depth + 1, MaxNum, MaxDepth);
	FlatBVHNodes[ID].Right = rightID;
	return ID;
}

void KH_FlatBVHNode::Hit(std::vector<KH_BVHHitInfo>& HitInfos, std::vector<KH_FlatBVHNode>& FlatBVHNodes, KH_Ray& Ray)
{
	KH_AABBHitInfo AABBHit = AABB.Hit(Ray);
	if (!AABBHit.bIsHit) return;

	if (bIsLeaf)
	{
		KH_BVHHitInfo BVHHitInfo;
		BVHHitInfo.BeginIndex = Offset;
		BVHHitInfo.EndIndex = Offset + Size;
		BVHHitInfo.HitTime = AABBHit.HitTime;
		BVHHitInfo.bIsHit = true;
		HitInfos.push_back(BVHHitInfo);
		return;
	}

	if (Left != KH_FLAT_BVH_NULL_NODE)  FlatBVHNodes[Left].Hit(HitInfos, FlatBVHNodes, Ray);
	if (Right != KH_FLAT_BVH_NULL_NODE) FlatBVHNodes[Right].Hit(HitInfos, FlatBVHNodes, Ray);
}


KH_FlatBVH::KH_FlatBVH(uint32_t MaxBVHDepth, uint32_t MaxLeafPrimitives, KH_BVH_BUILD_MODE BuildMode)
	:KH_IBVH(MaxBVHDepth, MaxLeafPrimitives, BuildMode)
{
}

void KH_FlatBVH::BindAndBuild(std::vector<KH_SceneObject>& Objects)
{
	CollectPrimitives(Objects);

	Root = KH_FLAT_BVH_NULL_NODE;
	BVHNodes.clear();

	BuildBVH();
	FillModelMatrices(MaxBVHDepth);

	//std::string DebugMessage = std::format("Model Range : [({},{},{}),({},{},{})]",
	//	BVHNodes[Root].AABB.MinPos.x, BVHNodes[Root].AABB.MinPos.y, BVHNodes[Root].AABB.MinPos.z,
	//	BVHNodes[Root].AABB.MaxPos.x, BVHNodes[Root].AABB.MaxPos.y, BVHNodes[Root].AABB.MaxPos.z);
	//LOG_D(DebugMessage);
}

std::vector<KH_BVHHitInfo> KH_FlatBVH::Hit(KH_Ray& Ray)
{
	std::vector<KH_BVHHitInfo> HitInfos;
	if (Root != KH_FLAT_BVH_NULL_NODE)
		BVHNodes[Root].Hit(HitInfos, BVHNodes, Ray);
	return HitInfos;
}

void KH_FlatBVH::FillModelMatrices(uint32_t TargetDepth)
{
	ModelMats.clear();
	MatCount = 0;

	FillModelMatrices_Inner(Root, 0, TargetDepth);

	UpdateModelMatsSSBO();
}

void KH_FlatBVH::FillModelMatrices_Inner(int FlatBVHNodeID, uint32_t CurrentDepth, uint32_t TargetDepth)
{
	if (FlatBVHNodeID == KH_FLAT_BVH_NULL_NODE)
		return;

	if (CurrentDepth == TargetDepth || BVHNodes[FlatBVHNodeID].bIsLeaf)
	{
		//TODO : Do some optimization
		ModelMats.push_back(BVHNodes[FlatBVHNodeID].AABB.GetModelMatrix());
		MatCount += 1;
		return;
	}

	FillModelMatrices_Inner(BVHNodes[FlatBVHNodeID].Left, CurrentDepth + 1, TargetDepth);
	FillModelMatrices_Inner(BVHNodes[FlatBVHNodeID].Right, CurrentDepth + 1, TargetDepth);
}

void KH_FlatBVH::BuildBVH()
{
	switch (BuildMode)
	{
	case KH_BVH_BUILD_MODE::Base:
		this->Root = KH_FlatBVHNode::BuildNode(Primitives, BVHNodes, 0, PrimitiveCount, 0, MaxLeafPrimitives, MaxBVHDepth);
		break;
	case KH_BVH_BUILD_MODE::SAH:
		this->Root = KH_FlatBVHNode::BuildNodeSAH(Primitives, BVHNodes, 0, PrimitiveCount, 0, MaxLeafPrimitives, MaxBVHDepth);
		break;
	}
}
