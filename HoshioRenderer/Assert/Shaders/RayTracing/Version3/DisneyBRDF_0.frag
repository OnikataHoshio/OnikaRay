#version 460 core
out vec4 FragColor;

in vec3 CanvasPos; 

struct Primitive{
    vec4 P1, P2, P3;
    vec4 N1, N2, N3;
    ivec2 PrimitiveType;
    ivec2 MaterialSlot;
};

struct EncodedBRDFMaterial{
	vec4 Emissive;  
	vec4 BaseColor;
    vec4 Param1;
    vec4 Param2;
    vec4 Param3;
};

struct BRDFMaterial{
	vec3 Emissive; 
	vec3 BaseColor;
	float Subsurface;
	float Metallic;
	float Specular;
	float SpecularTint;
	float Roughness;
	float Anisotropic;
	float Sheen;
	float SheenTint;
	float Clearcoat;
	float ClearcoatGloss;
};

struct LBVHNode{
    ivec4 Param1;
    ivec4 Param2;
    vec4 AABB_MinPos;
    vec4 AABB_MaxPos;
};

layout(std430, binding = 0) buffer PrimitiveSSBO { Primitive Primitives[]; };
layout(std430, binding = 1) buffer Morton3DBuffer { uvec2 SortedMorton3D[]; };
layout(std430, binding = 2) buffer LBVHNodeBuffer { LBVHNode LBVHNodes[]; };
layout(std430, binding = 3) buffer AuxiliaryBuffer { ivec4 Root; };
layout(std430, binding = 4) buffer EncodedBRDFMaterialSSBO{ EncodedBRDFMaterial Materials[]; };

layout(std140, binding = 5) uniform CameraBlock {
    vec4 AspectAndFovy; // x: Aspect, y: Fovy
    vec4 Position;
    vec4 Right;
    vec4 Up;
    vec4 Front;
} UCameraParam;


uniform sampler2D uLastFrame;
uniform sampler2D uSkybox;
uniform int uLBVHNodeCount;
uniform uint uFrameCounter;
uniform uvec2 uResolution; 


const vec3 SkyColor = vec3(0.05, 0.05, 0.05);

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
	vec3 GeoNormal;
    vec3 ShadeNormal;
    int MaterialSlot;
};

#define INF             114514.0
#define EPS             1e-8
#define PI              3.14159265358979323846

uint rngState;

BRDFMaterial DecodeBRDFMaterial(int MaterialSlotID)
{
    BRDFMaterial Material;
    EncodedBRDFMaterial EncodedMaterial = Materials[MaterialSlotID];

    Material.BaseColor = EncodedMaterial.BaseColor.xyz;
    Material.Emissive = EncodedMaterial.Emissive.xyz;
    Material.Subsurface = EncodedMaterial.Param1.x;
    Material.Metallic = EncodedMaterial.Param1.y;
    Material.Specular = EncodedMaterial.Param1.z;
    Material.SpecularTint = EncodedMaterial.Param1.w;
    Material.Roughness = EncodedMaterial.Param2.x;
    Material.Anisotropic = EncodedMaterial.Param2.y;
    Material.Sheen = EncodedMaterial.Param2.z;
    Material.SheenTint = EncodedMaterial.Param2.w;
    Material.Clearcoat = EncodedMaterial.Param3.x;
    Material.ClearcoatGloss = EncodedMaterial.Param3.y;

    return Material;
}

uvec2 GetFragCoord()
{
    vec2 uv = (CanvasPos.xy + 1.0)/2.0;
    return uvec2(uv * uResolution);
}

void InitRNG()
{
    uvec2 fc = GetFragCoord();
    rngState = uint(fc.x) * 1973u + uint(fc.y) * 9277u + uint(uFrameCounter) * 26699u + 1u;
}

uint wang_hash(uint seed) {
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float rand()
{
    rngState = wang_hash(rngState);
    return float(rngState) / 4294967296.0;
}

vec3 SampleHemisphere() {
    float z = rand();
    float r = max(0, sqrt(1.0 - z*z));
    float phi = 2.0 * PI * rand();
    return vec3(r * cos(phi), r * sin(phi), z);
}

vec3 ToNormalHemisphere(vec3 v, vec3 N) {
    N = normalize(N);
    vec3 helper = vec3(1, 0, 0);
    if(abs(N.x)>0.999) helper = vec3(0, 0, 1);
    vec3 tangent = normalize(cross(N, helper));
    vec3 bitangent = normalize(cross(N, tangent));
    return normalize(v.x * tangent + v.y * bitangent + v.z * N);
}

vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv /= vec2(2.0 * PI, PI);
    uv += 0.5;
    return uv;
}

vec3 SampleSkybox(vec3 v) {
    vec2 uv = SampleSphericalMap(normalize(v));
    vec3 color = texture2D(uSkybox, uv).rgb;
    return color;
}

vec3 GetRayDirection(vec2 uv)
{
    float scale = tan(radians(UCameraParam.AspectAndFovy.y) * 0.5);

    vec2 pixelSize = 2.0 / uResolution;

    uv += pixelSize * (rand() - 0.5);

    vec3 DirWorldSpace = (uv.x * UCameraParam.AspectAndFovy.x * scale) * UCameraParam.Right.xyz +
        (uv.y * scale) * UCameraParam.Up.xyz +
        UCameraParam.Front.xyz;

    return normalize(DirWorldSpace);
}

HitResult HitTriangle(int Primitive_index, Ray ray)
{
    HitResult hit_result;
    hit_result.bIsHit = false;
    hit_result.Distance = INF;
    hit_result.MaterialSlot = -1;

    Primitive Primitive = Primitives[Primitive_index];

    vec3 p1 = Primitive.P1.xyz;
    vec3 p2 = Primitive.P2.xyz;
    vec3 p3 = Primitive.P3.xyz;

    vec3 edge1 = p2 - p1;
    vec3 edge2 = p3 - p1;

    vec3 Ng = normalize(cross(edge1, edge2));

    vec3 pvec = cross(ray.Direction, edge2);
    float det = dot(edge1, pvec);

    if (abs(det) < EPS) return hit_result;

    float invDet = 1.0 / det;

    vec3 tvec = ray.Start - p1;
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
    hit_result.MaterialSlot = Primitive.MaterialSlot.x;

    float w1 = 1.0 - u - v;
    vec3 Ns = normalize(w1 * Primitive.N1.xyz + u * Primitive.N2.xyz + v * Primitive.N3.xyz);

    if (dot(Ng, ray.Direction) > 0.0)
        Ng = -Ng;

    if (dot(Ns, Ng) < 0.0)
        Ns = -Ns;

    hit_result.GeoNormal = Ng;
    hit_result.ShadeNormal = Ns;

    return hit_result;
}

HitResult Hit(Ray ray, int l, int r) 
{
    HitResult hit_result;
    hit_result.bIsHit = false;    
    hit_result.Distance = INF;
    hit_result.MaterialSlot = -1; 

	for (int i = l; i <= r; i++)
	{
		HitResult temp = HitTriangle(int(SortedMorton3D[i].y), ray);
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

    stack[top++] = Root.x;

    while(top > 0)
    {
        int cur_node_idx = stack[--top];
        
        if(cur_node_idx < 0 || cur_node_idx >= uLBVHNodeCount) 
            continue;

        int left = LBVHNodes[cur_node_idx].Param1.x;
        int right = LBVHNodes[cur_node_idx].Param1.y;
        int isLeaf = LBVHNodes[cur_node_idx].Param1.z;
        int front = LBVHNodes[cur_node_idx].Param2.x;
        int back = LBVHNodes[cur_node_idx].Param2.y;

        if(isLeaf == 1)
        {
            HitResult temp = Hit(ray, front , back); 
            if (temp.bIsHit && temp.Distance < hit_result.Distance)
			    hit_result = temp;
            continue;
        }
        
        float t_left = -INF;
        float t_right = -INF;

        if(left >= 0 && left < uLBVHNodeCount)
            t_left = HitAABB(left, ray);
        if(right >= 0 && right < uLBVHNodeCount)
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
        else if (t_left > 0.0) {
            stack[top++] = left;
        } else if (t_right > 0.0) { 
            stack[top++] = right;
        }
    }
    
    return hit_result;
}

float sqr(float x) { return x*x; }

float SchlickFresnel(float u)
{
    float m = clamp(1-u, 0, 1);
    float m2 = m*m;
    return m2*m2*m; // pow(m,5)
}

float GTR1(float NdotH, float a)
{
    if (a >= 1) return 1/PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (PI*log(a2)*t);
}

float GTR2(float NdotH, float a)
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}

float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}

vec3 mon2lin(vec3 x)
{
    return pow(x, vec3(2.2));
}

vec3 BRDF( vec3 L, vec3 V, vec3 N,  int MaterialSlotID)
{
    BRDFMaterial Mat = DecodeBRDFMaterial(MaterialSlotID);

    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    if (NdotL < 0 || NdotV < 0) return vec3(0);

    vec3 H = normalize(L+V);
    float NdotH = dot(N,H);
    float LdotH = dot(L,H);
    
    vec3 Cdlin = Mat.BaseColor;
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.
    
    vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); // normalize lum. to isolate hue+sat
    vec3 Cspec0 = mix(Mat.Specular*.08*mix(vec3(1), Ctint, Mat.SpecularTint), Cdlin, Mat.Metallic);
    vec3 Csheen = mix(vec3(1), Ctint, Mat.SheenTint);

    // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    // and mix in diffuse retro-reflection based on roughness
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * Mat.Roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on roughness
    float Fss90 = LdotH*LdotH*Mat.Roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

    vec3 diffuse = (1.0/PI) * mix(Fd, ss, Mat.Subsurface) * Cdlin;

    float alpha = Mat.Roughness * Mat.Roughness;
    float Ds =  GTR2(NdotH, alpha);
    float FH = SchlickFresnel(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs = smithG_GGX(NdotL, Mat.Roughness) * smithG_GGX(NdotV, Mat.Roughness);

    vec3 specular = Gs * Fs * Ds;

    // sheen
    vec3 Fsheen = FH * Mat.Sheen * Csheen;
    diffuse += Fsheen;

    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, mix(.1,.001,Mat.ClearcoatGloss));
    float Fr = mix(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);

    return diffuse  * (1.0 - Mat.Metallic) + specular + .25 * Mat.Clearcoat * Gr * Fr * Ds;
}

vec3 PathTracing(Ray ray, int maxBounce)
{
    vec3 finalColor = vec3(0.0);
    vec3 throughput = vec3(1.0);
    Ray currRay = ray;

    for(int i = 0; i < maxBounce; i++)
    {
        HitResult hit_result = HitBVH(currRay);

        if(!hit_result.bIsHit)
        {
            finalColor += throughput * SkyColor;
            break;
        }

        EncodedBRDFMaterial material = Materials[hit_result.MaterialSlot];
        finalColor += throughput * material.Emissive.xyz;

        vec3 Ng = normalize(hit_result.GeoNormal);
        vec3 Ns = normalize(hit_result.ShadeNormal);

        vec3 V = -currRay.Direction;

        vec3 wi = ToNormalHemisphere(SampleHemisphere(), Ng);

        float PDF = 1.0 / (2.0 * PI);
        float cosTheta = max(dot(wi, Ns), 0.0);

        throughput *= BRDF(wi, V, Ns, hit_result.MaterialSlot) * cosTheta / PDF;

        currRay.Start = hit_result.HitPoint + Ng * 1e-4;
        currRay.Direction = wi;

        if(i >= 3)
        {
            float p = clamp(max(throughput.r, max(throughput.g, throughput.b)), 0.05, 0.95);
            if(rand() > p)
                break;
            throughput /= p;
        }
    }

    return finalColor;
}

void main()
{
    InitRNG();

    Ray ray;
    ray.Start = UCameraParam.Position.xyz;
    ray.Direction = GetRayDirection(CanvasPos.xy);

    vec4 Color = vec4(PathTracing(ray, 8), 1.0);

    vec4 LastFrameColor = texture(uLastFrame, (CanvasPos.xy + 1.0)/2.0);

    if(uFrameCounter < 1024)
        FragColor = mix(LastFrameColor, Color, 1.0/float(uFrameCounter+1));
    else
        FragColor = LastFrameColor;
}



