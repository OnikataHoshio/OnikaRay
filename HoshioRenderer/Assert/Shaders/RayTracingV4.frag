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

struct LBVHNode{
    ivec4 Param1;
    ivec4 Param2;
    vec4 AABB_MinPos;
    vec4 AABB_MaxPos;
};


layout(std430, binding = 0) buffer TriangleSSBO {
    Triangle Triangles[];
};

layout(std430, binding = 1) buffer BRDFMaterialSSBO{
    BRDFMaterial Materials[];
};

layout(std430, binding = 2) buffer LBVHNodeSSBO{
    LBVHNode LBVHNodes[];
};

uniform CameraParam UCameraParam;

uniform int TriangleCount;

uniform int LBVHNodeCount;

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

float HitAABB(int node_index, Ray ray)
{
    vec3 AABB_MinPos = vec3(LBVHNodes[node_index].AABB_MinPos);
    vec3 AABB_MaxPos = vec3(LBVHNodes[node_index].AABB_MaxPos);

    vec3 invDir = 1.0 / ray.Direction;
    
    vec3 t0s = (AABB_MinPos - ray.Start) * invDir;
    vec3 t1s = (AABB_MaxPos - ray.Start) * invDir;
    
    vec3 tmin = min(t0s, t1s);
    vec3 tmax = max(t0s, t1s);
    
    float t_start = max(tmin.x, max(tmin.y, tmin.z));
    float t_end = min(tmax.x, min(tmax.y, tmax.z));
    
    if (t_start <= t_end && t_end > 0.0)
    {
        return t_start > 0.0 ? t_start : t_end; 
    }

    return -1.0;
}

HitResult HitBVH(Ray ray)
{
    HitResult hit_result;
    hit_result.bIsHit = false;    
    hit_result.Distance = INF;
    hit_result.MaterialSlot = -1; 

    int stack[256];
    int top = 0;

    stack[top++] = 0;

    while(top > 0)
    {
        int cur_node_idx = stack[--top];
        
        if(cur_node_idx < 0 || cur_node_idx >= LBVHNodeCount) 
            continue;

        int left = LBVHNodes[cur_node_idx].Param1.x;
        int right = LBVHNodes[cur_node_idx].Param1.y;
        int isLeaf = LBVHNodes[cur_node_idx].Param2.x;
        int offset = LBVHNodes[cur_node_idx].Param2.y;
        int size = LBVHNodes[cur_node_idx].Param2.z;

        if(isLeaf == 1)
        {
            HitResult temp = Hit(ray, offset , offset+size); 
            if (temp.bIsHit && temp.Distance < hit_result.Distance)
			    hit_result = temp;
            continue;
        }
        
        float t_left = -INF;
        float t_right = -INF;

        if(left > 0 && left < LBVHNodeCount)
            t_left = HitAABB(left, ray);
        if(right > 0 && right < LBVHNodeCount)
            t_right = HitAABB(right, ray);

        if(t_left > 0 && t_right > 0)
        {
            if(t_left > t_right)
            {
                stack[top++] = left;
                stack[top++] = right;
            }else{
                stack[top++] = right;
                stack[top++] = left;
            }
        }
        else if(t_left > 0)
        {
            stack[top++] = left;
        }
        else
        {
            stack[top++] = right;
        }
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

    HitResult hit_result = HitBVH(ray);

    if(hit_result.bIsHit)
    {
        uint material_slot = hit_result.MaterialSlot;
        BRDFMaterial material = Materials[material_slot];
        FragColor = material.BaseColor;
    }else{
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}



