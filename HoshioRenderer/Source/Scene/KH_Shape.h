#pragma once

#include "Hit/KH_AABB.h"
#include "Pipeline/KH_Material.h"

struct KH_HitResult;
class KH_Ray;

class KH_Primitive;
using KH_ScenePrimitive = std::unique_ptr<KH_Primitive>;

enum class KH_InspectorCommitType
{
	None,
	RebuildBVH,
	UpdateMaterial,
	ReuploadSceneData
};

struct KH_InspectorEditResult
{
	bool bValueChanged = false;
	KH_InspectorCommitType CommitType = KH_InspectorCommitType::None;
};
enum class KH_PrimitiveType : int
{
	Triangle = 0,
	Sphere = 1
};

constexpr int KH_MATERIAL_UNDEFINED_SLOT = -1;

struct KH_PrimitiveEncoded
{
	union {
		struct {
			glm::vec4 P1, P2, P3;
			glm::vec4 N1, N2, N3;
		} Triangle;

		struct {
			glm::vec4 Center;
			glm::vec4 Radius;
			glm::vec4 Reserved1;
			glm::vec4 Reserved2;
			glm::vec4 Reserved3;
			glm::vec4 Reserved4;
		} Sphere;
	};

	glm::ivec2 PrimitiveType;
	glm::ivec2 MaterialSlotID;
};


class KH_Object
{
protected:
	glm::vec3 Position = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 Scale = glm::vec3(1.0f, 1.0f, 1.0f);

	glm::vec3 Rotation = glm::vec3(0.0f, 0.0f, 0.0f);

	glm::quat RotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	glm::vec3 GizmoPivotLocal = glm::vec3(0.0f);

	glm::mat4 ModelMatrix = glm::mat4(1.0f);
	glm::mat3 NormalMatrix = glm::mat3(1.0f);

public:
	KH_Object() = default;
	virtual ~KH_Object() = default;

	KH_Object(const KH_Object&) = default;
	KH_Object& operator=(const KH_Object&) = default;
	KH_Object(KH_Object&&) noexcept = default;
	KH_Object& operator=(KH_Object&&) noexcept = default;

	virtual uint32_t GetPrimitiveCount() const = 0;
	virtual void EncodePrimitives(std::vector<KH_PrimitiveEncoded>& outPrimitives) const = 0;
	virtual void CollectPrimitives(std::vector<KH_ScenePrimitive>& outPrimitives) const = 0;
	virtual void CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const = 0;
	virtual const KH_AABB& GetAABB() const = 0;

	virtual KH_InspectorEditResult DrawInspector();

public:
	void SetPosition(const glm::vec3& InPosition);
	void SetPosition(float X, float Y, float Z);

	void SetScale(const glm::vec3& InScale);
	void SetScale(float X, float Y, float Z);
	void SetUniformScale(float S);

	void SetRotation(const glm::vec3& InRotation);
	void SetRotation(float Pitch, float Yaw, float Roll);

	void SetRotationQuat(const glm::quat& InRotationQuat);

	void SetTransform(const glm::vec3& InPosition, const glm::vec3& InRotation, const glm::vec3& InScale);
	void SetTransform(const glm::vec3& InPosition, const glm::quat& InRotationQuat, const glm::vec3& InScale);

	void Translate(const glm::vec3& Delta);
	void Translate(float X, float Y, float Z);

	void Rotate(const glm::vec3& DeltaRotation);
	void Rotate(float Pitch, float Yaw, float Roll);

	void AddScale(const glm::vec3& DeltaScale);
	void AddScale(float X, float Y, float Z);

	const glm::vec3& GetPosition() const;
	const glm::vec3& GetScale() const;
	const glm::vec3& GetRotation() const;
	const glm::quat& GetRotationQuat() const;

	glm::mat4 GetModelMatrix() const;
	glm::mat3 GetNormalMatrix() const;

	void SetGizmoPivotLocal(const glm::vec3& InPivot);
	const glm::vec3& GetGizmoPivotLocal() const;

	glm::vec3 GetGizmoPivotWorld() const;

	glm::mat4 GetGizmoMatrix() const;

	static glm::mat3 GetNormalMatrix(glm::mat4 Model);

protected:
	glm::mat4 UpdateModelMatrix();
	glm::mat3 UpdateNormalMatrix();

	virtual void OnTransformChanged();

private:
	static glm::quat EulerDegreesToQuatXYZ(const glm::vec3& degreesXYZ);
	static glm::vec3 QuatToEulerDegreesXYZ(const glm::quat& q);
};

class KH_Hitable : public KH_Object {
protected:
	KH_AABB AABB;
public:
	KH_Hitable() = default;
	virtual ~KH_Hitable() override = default;
	virtual KH_HitResult Hit(const KH_Ray& Ray) const = 0;
	virtual const KH_AABB& GetAABB() const override;
	virtual void CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const override;
protected:
	virtual void UpdateAABB() = 0;
	virtual void OnTransformChanged() override;
};

class KH_Primitive : public KH_Hitable
{
public:
	KH_PrimitiveType PrimitiveType;
	int MaterialSlotID = 0;

	glm::vec3 GetMinPos() const;
	glm::vec3 GetMaxPos() const;
	glm::vec3 GetAABBCenter() const;
	virtual glm::vec3 GetCenterWS() const = 0;
	virtual uint32_t GetPrimitiveCount() const override = 0;

	virtual void CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const override = 0;
	

	static bool Cmpx(const KH_Primitive& p1, const KH_Primitive& p2);
	static bool Cmpy(const KH_Primitive& p1, const KH_Primitive& p2);
	static bool Cmpz(const KH_Primitive& p1, const KH_Primitive& p2);

	static bool CmpxPtr(const std::unique_ptr<KH_Primitive>& p1,
		const std::unique_ptr<KH_Primitive>& p2);
	static bool CmpyPtr(const std::unique_ptr<KH_Primitive>& p1,
		const std::unique_ptr<KH_Primitive>& p2);
	static bool CmpzPtr(const std::unique_ptr<KH_Primitive>& p1,
		const std::unique_ptr<KH_Primitive>& p2);

protected:
	virtual void UpdateAABB() override = 0;
};

class KH_Triangle : public KH_Primitive
{
	struct KH_TriangleHitInfo
	{
		bool bIsHit = false;
		float w1 = 0.0f, w2 = 0.0f, w3 = 0.0f;
	};

	struct KH_TriangleWorldData
	{
		glm::vec3 P1, P2, P3;
		glm::vec3 N1, N2, N3;
	};

public:
	glm::vec3 P1, P2, P3;
	glm::vec3 N1, N2, N3;
	glm::vec3 CenterWS;
	glm::vec3 NormalWS;


	KH_Triangle();
	KH_Triangle(glm::vec3 P1, glm::vec3 P2, glm::vec3 P3);
	KH_Triangle(glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, glm::vec3 N1, glm::vec3 N2, glm::vec3 N3);
	KH_Triangle(const KH_Triangle&) = default;
	KH_Triangle& operator=(const KH_Triangle&) = default;

	virtual KH_HitResult Hit(const KH_Ray& Ray) const override;

	virtual uint32_t GetPrimitiveCount() const override;
	virtual void EncodePrimitives(std::vector<KH_PrimitiveEncoded>& outPrimitives) const override;
	virtual void CollectPrimitives(std::vector<KH_ScenePrimitive>& outPrimitives) const override;
	virtual void CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const override;
	
	virtual glm::vec3 GetCenterWS() const override;

private:

	virtual void UpdateAABB() override;
	virtual void UpdateOtherData();
	virtual void OnTransformChanged() override;

	KH_TriangleHitInfo CheckHitInfo(glm::vec3 HitPoint);
	KH_TriangleWorldData GetWorldData() const;
};





