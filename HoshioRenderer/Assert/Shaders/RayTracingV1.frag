#version 430 core
out vec4 FragColor;

in vec3 CanvasPos; 

struct CameraParam
{
    vec3 Position;
    vec3 Right;
    vec3 Up;
    vec3 Front;
    float Aspect;
    float Fovy;
};

struct Triangle{
    vec4 P1, P2, P3;
    vec4 N1, N2, N3;
    ivec4 MaterialSlot;
};

struct BRDFMaterial{
	vec4 Emissive;  
	vec4 BaseColor;
    vec4 Param1;
    vec4 Param2;
    vec4 Param3;
};

layout(std430, binding = 0) buffer TriangleSSBO {
    Triangle Triangles[];
};

layout(std430, binding = 1) buffer BRDFMaterialSSBO{
    BRDFMaterial Materials[];
};

uniform CameraParam UCameraParam;

uniform int TriangleCount;

struct Ray
{
	vec3 Start;
	vec3 Direction;
};

struct HitResult {
	bool bIsHit;
    // bool BIsInside;
	float Distance;
	vec3 HitPoint;
	vec3 Normal;
    int MaterialSlot;
};

#define INF             114514.0
#define EPS             1e-6

vec3 GetRayDirection(float u, float v)
{
    float scale = tan(radians(UCameraParam.Fovy) * 0.5);

    vec3 DirWorldSpace = (u * UCameraParam.Aspect * scale) * UCameraParam.Right +
        (v * scale) * UCameraParam.Up +
        UCameraParam.Front;

    return normalize(DirWorldSpace);
}

HitResult HitTriangle(int triangle_index, Ray ray)
{
    HitResult hit_result;
    hit_result.bIsHit = false;    
    hit_result.Distance = INF;
    hit_result.MaterialSlot = -1; 

    Triangle triangle = Triangles[triangle_index];

    vec3 edge1 = vec3(triangle.P2 - triangle.P1);
    vec3 edge2 = vec3(triangle.P3 - triangle.P1);
    vec3 pvec = cross(ray.Direction, edge2);
    float det = dot(edge1, pvec);

    if (abs(det) < EPS) return hit_result;

    float invDet = 1.0 / det;

    vec3 tvec = ray.Start - vec3(triangle.P1);
    float u = dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0) return hit_result;

    vec3 qvec = cross(tvec, edge1);
    float v = dot(ray.Direction, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) return hit_result;

    float t = dot(edge2, qvec) * invDet;
    if (t < EPS) return hit_result; 

    hit_result.bIsHit = true;
    hit_result.Distance = t;
    hit_result.HitPoint = ray.Start + t * ray.Direction;
    hit_result.MaterialSlot = triangle.MaterialSlot.x;

    float w1 = 1.0 - u - v;
    hit_result.Normal = normalize(vec3((w1 * triangle.N1 + u * triangle.N2 + v * triangle.N3)));

    return hit_result;
}

HitResult Hit(Ray ray, int l, int r) 
{
    HitResult hit_result;
    hit_result.bIsHit = false;    
    hit_result.Distance = INF;
    hit_result.MaterialSlot = -1; 

	for (int i = l; i < r; i++)
	{
		HitResult temp = HitTriangle(i, ray);
		if (temp.bIsHit && temp.Distance < hit_result.Distance)
			hit_result = temp;
	}
    return hit_result;
}

void main()
{
    float u = CanvasPos.x;
    float v = CanvasPos.y;
    Ray ray;
    ray.Start = UCameraParam.Position;
    ray.Direction = GetRayDirection(u, v);

    HitResult hit_result = Hit(ray, 0, TriangleCount);

    if(hit_result.bIsHit)
    {
        uint material_slot = hit_result.MaterialSlot;
        BRDFMaterial material = Materials[material_slot];
        FragColor = material.BaseColor;
    }else{
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }

}