#pragma once

#include "KH_Common.h"

struct KH_BaseMaterial {
public:
	bool bIsEmissive = false;
	glm::vec3 Color = glm::vec3();
	float SpecularRate = 0.0f;
	float ReflectRoughness = 1.0f;

	float RefractRate = 0.0f;
	float Eta = 1.0f;
	float RefractRoughness = 0.0f;
};

struct KH_BRDFMaterial {
	glm::vec3 Emissive = glm::vec3(0, 0, 0);  // 作为光源时的发光颜色
	glm::vec3 BaseColor = glm::vec3(1, 1, 1);
	float Subsurface = 0.0;
	float Metallic = 0.0;
	float Specular = 0.0;
	float SpecularTint = 0.0;
	float Roughness = 0.0;
	float Anisotropic = 0.0;
	float Sheen = 0.0;
	float SheenTint = 0.0;
	float Clearcoat = 0.0;
	float ClearcoatGloss = 0.0;
	float IOR = 1.0;
	float Transmission = 0.0;
};

class KH_DefaultMaterial : public KH_Singleton<KH_DefaultMaterial>
{
	friend class KH_Singleton<KH_DefaultMaterial>;

private:
	KH_DefaultMaterial();
	virtual ~KH_DefaultMaterial() = default;

	void Initialize();
	void Deinitialize();

	void InitBaseMaterial1();
	void InitBaseMaterial2();

public:
	KH_BaseMaterial BaseMaterial1;
	KH_BaseMaterial BaseMaterial2;
};



