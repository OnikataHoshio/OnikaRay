#pragma once

#include "KH_Common.h"



class KH_Ray;

class KH_Triangle;

struct KH_AABBHitInfo
{
	float HitTime = std::numeric_limits<float>::max();
	bool bIsHit = false;
};

class KH_AABB
{
public:
	KH_AABB(glm::vec3 MinPos = glm::vec3(std::numeric_limits<float>::max()), glm::vec3 MaxPos = glm::vec3(-std::numeric_limits<float>::max()));

	KH_AABBHitInfo Hit(const KH_Ray& Ray) const;

	bool CheckOverlap(KH_AABB& Other);

	void Merge(const KH_AABB& Other);

	void Merge(const glm::vec3& OtherMinPos, const glm::vec3& OtherMaxPos);

	void Merge(const KH_Triangle& Triangle);

	void Update(const KH_Triangle& Triangle);

	void Reset();

	bool IsInvalid() const;

	glm::vec3 GetSize() const;

	glm::vec3 GetInvSize() const;

	glm::vec3 GetCenter() const;

	glm::vec3 GetScale() const;

	glm::mat4 GetModelMatrix() const;

	float GetSurfaceArea() const;

	static float ComputeSurfaceArea(glm::vec3 MinPos, glm::vec3 MaxPos);

	glm::vec3 MinPos = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 MaxPos = glm::vec3(0.0f, 0.0f, 0.0f);

};

