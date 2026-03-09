#include "KH_Scene.h"

#include "Editor/KH_Editor.h"
#include "Pipeline/KH_Shader.h"

#include "Utils/KH_DebugUtils.h"

void KH_Scene::SetSSBOs()
{
	static std::vector<KH_TriangleEncoded> TriangleEncodeds;
	static std::vector<KH_BRDFMaterialEncoded> BSDFMaterialEncodeds;
	static std::vector<KH_LBVHNodeEncoded> LBVHNodeEncodeds;

	TriangleEncodeds.clear();
	BSDFMaterialEncodeds.clear();
	LBVHNodeEncodeds.clear();

	TriangleEncodeds = EncodeTriangles();
	BSDFMaterialEncodeds = EncodeBSDFMaterials();
	LBVHNodeEncodeds = EncodeLBVHNodes();

	std::string DebugMessage = std::format("KH_TriangleEncoded size : {} Byte", sizeof(KH_TriangleEncoded));
	LOG_D(DebugMessage);
	DebugMessage = std::format("KH_BSDFMaterialEncoded size : {} Byte", sizeof(KH_BRDFMaterialEncoded));
	LOG_D(DebugMessage);
	DebugMessage = std::format("KH_LBVHNodeEcoded size : {} Byte", sizeof(KH_LBVHNodeEncoded));
	LOG_D(DebugMessage);

	Triangle_SSBO.SetData(TriangleEncodeds);
	Material_SSBO.SetData(BSDFMaterialEncodeds);
	LBVHNode_SSBO.SetData(LBVHNodeEncodeds);
}

//void KH_Scene::SetRayTracingV1() const
//{
//	const KH_Shader& Shader = KH_ExampleShaders::Instance().RayTracingShader1;
//	const KH_Camera& Camera = KH_Editor::Instance().Camera;
//
//	Triangle_SSBO.Bind();
//	Material_SSBO.Bind();
//
//
//	Shader.SetInt("TriangleCount", Triangles.size());
//	Shader.SetVec3("UCameraParam.Position", Camera.Position);
//	Shader.SetVec3("UCameraParam.Right", Camera.Right);
//	Shader.SetVec3("UCameraParam.Up", Camera.Up);
//	Shader.SetVec3("UCameraParam.Front", Camera.Front);
//	Shader.SetFloat("UCameraParam.Aspect", Camera.Aspect);
//	Shader.SetFloat("UCameraParam.Fovy", Camera.Fovy);
//}

void KH_Scene::SetRayTracingParam() const
{
	const KH_Shader& Shader = KH_ExampleShaders::Instance().RayTracingShader1;
	const KH_Camera& Camera = KH_Editor::Instance().Camera;

	Triangle_SSBO.Bind();
	Material_SSBO.Bind();
	LBVHNode_SSBO.Bind();

	Shader.SetInt("TriangleCount", Triangles.size());

	Shader.SetInt("LBVHNodeCount", BVH.LBVHNodes.size());

	Shader.SetVec3("UCameraParam.Position", Camera.Position);
	Shader.SetVec3("UCameraParam.Right", Camera.Right);
	Shader.SetVec3("UCameraParam.Up", Camera.Up);
	Shader.SetVec3("UCameraParam.Front", Camera.Front);
	Shader.SetFloat("UCameraParam.Aspect", Camera.Aspect);
	Shader.SetFloat("UCameraParam.Fovy", Camera.Fovy);

}

KH_Scene::KH_Scene()
{
	Triangle_SSBO.SetBindPoint(0);
	Material_SSBO.SetBindPoint(1);
	LBVHNode_SSBO.SetBindPoint(2);
}

KH_Scene::KH_Scene(uint32_t MaxBVHDepth, uint32_t MaxLeafTriangles, KH_BVH_BUILD_MODE BuildMode)
	:BVH(MaxBVHDepth, MaxLeafTriangles)
{
	BVH.BuildMode = BuildMode;
	Triangle_SSBO.SetBindPoint(0);
	Material_SSBO.SetBindPoint(1);
	LBVHNode_SSBO.SetBindPoint(2);
}

void KH_Scene::LoadObj(const std::string& path, float scale, int MaterialSlot)
{
	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(path)) {
		std::cerr << "TinyObjReader Error: " << reader.Error() << std::endl;
		return;
	}

	const auto& attrib = reader.GetAttrib();
	const auto& shapes = reader.GetShapes();


	size_t totalIndices = 0;
	for (const auto& shape : shapes) totalIndices += shape.mesh.indices.size();
	Triangles.reserve(totalIndices / 3);

	std::vector<glm::vec3> vertexNormals(attrib.vertices.size() / 3, glm::vec3(0.0f));

	for (const auto& shape : shapes) {
		size_t index_offset = 0;
		for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
			tinyobj::index_t idx0 = shape.mesh.indices[index_offset + 0];
			tinyobj::index_t idx1 = shape.mesh.indices[index_offset + 1];
			tinyobj::index_t idx2 = shape.mesh.indices[index_offset + 2];

			glm::vec3 p0(attrib.vertices[3 * idx0.vertex_index + 0], attrib.vertices[3 * idx0.vertex_index + 1], attrib.vertices[3 * idx0.vertex_index + 2]);
			glm::vec3 p1(attrib.vertices[3 * idx1.vertex_index + 0], attrib.vertices[3 * idx1.vertex_index + 1], attrib.vertices[3 * idx1.vertex_index + 2]);
			glm::vec3 p2(attrib.vertices[3 * idx2.vertex_index + 0], attrib.vertices[3 * idx2.vertex_index + 1], attrib.vertices[3 * idx2.vertex_index + 2]);

			glm::vec3 faceNormal = glm::cross(p1 - p0, p2 - p0);

			vertexNormals[idx0.vertex_index] += faceNormal;
			vertexNormals[idx1.vertex_index] += faceNormal;
			vertexNormals[idx2.vertex_index] += faceNormal;

			index_offset += 3;
		}
	}

	for (const auto& shape : shapes) {
		size_t index_offset = 0;
		for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
			tinyobj::index_t idx[3];
			glm::vec3 p[3], n[3];

			for (size_t v = 0; v < 3; v++) {
				idx[v] = shape.mesh.indices[index_offset + v];
				p[v] = { attrib.vertices[3 * idx[v].vertex_index + 0], attrib.vertices[3 * idx[v].vertex_index + 1], attrib.vertices[3 * idx[v].vertex_index + 2] };

				n[v] = glm::normalize(vertexNormals[idx[v].vertex_index]);
			}

			Triangles.emplace_back(scale * p[0], scale * p[1], scale * p[2], n[0], n[1], n[2], MaterialSlot);
			index_offset += 3;
		}
	}
}


void KH_Scene::BindAndBuild()
{
	BVH.BindAndBuild(Triangles);

	SetSSBOs();
}

std::vector<KH_TriangleEncoded> KH_Scene::EncodeTriangles()
{
	const int nTriangles = Triangles.size();
	std::vector<KH_TriangleEncoded> TriangleEncodeds(nTriangles);

	for (int i = 0; i < nTriangles; i++)
	{
		TriangleEncodeds[i].P1 = glm::vec4(Triangles[i].P1, 1.0);
		TriangleEncodeds[i].P2 = glm::vec4(Triangles[i].P2, 1.0);
		TriangleEncodeds[i].P3 = glm::vec4(Triangles[i].P3, 1.0);

		TriangleEncodeds[i].N1 = glm::vec4(Triangles[i].N1, 1.0);
		TriangleEncodeds[i].N2 = glm::vec4(Triangles[i].N2, 1.0);
		TriangleEncodeds[i].N3 = glm::vec4(Triangles[i].N3, 1.0);

		TriangleEncodeds[i].MaterialSlot = glm::ivec4(Triangles[i].MaterialSlot, 0.0, 0.0,0.0);
 	}

	return TriangleEncodeds;
}

std::vector<KH_BRDFMaterialEncoded> KH_Scene::EncodeBSDFMaterials()
{
	const int nMaterials = Materials.size();
	std::vector<KH_BRDFMaterialEncoded> BSDFMaterialEncodeds(nMaterials);

	for (int i = 0; i < nMaterials; i++)
	{
		KH_BRDFMaterial& Mat = Materials[i];

		BSDFMaterialEncodeds[i].Emissive = glm::vec4(Mat.Emissive, 1.0);
		BSDFMaterialEncodeds[i].BaseColor = glm::vec4(Mat.BaseColor, 1.0);
		BSDFMaterialEncodeds[i].Param1 = glm::vec4(Mat.Subsurface, Mat.Metallic, Mat.Specular, Mat.SpecularTint);
		BSDFMaterialEncodeds[i].Param2 = glm::vec4(Mat.Roughness, Mat.Anisotropic, Mat.Sheen, Mat.SheenTint);
		BSDFMaterialEncodeds[i].Param3 = glm::vec4(Mat.Clearcoat, Mat.ClearcoatGloss, Mat.IOR, Mat.Transmission);
	}

	return BSDFMaterialEncodeds;
}

std::vector<KH_LBVHNodeEncoded> KH_Scene::EncodeLBVHNodes()
{
	const int nLBVHNodes = BVH.LBVHNodes.size();
	std::vector<KH_LBVHNodeEncoded> LBVHNodeEncoded(nLBVHNodes);

	for (int i = 0; i < nLBVHNodes; i++)
	{
		KH_LBVHNode& Node = BVH.LBVHNodes[i];

		LBVHNodeEncoded[i].Param1 = glm::ivec4(Node.Left, Node.Right, 0.0, 0.0);
		LBVHNodeEncoded[i].Param2 = glm::ivec4(Node.bIsLeaf ? 1 : 0, Node.Offset, Node.Size, 0);
		LBVHNodeEncoded[i].AABB_MinPos = glm::vec4(Node.AABB.MinPos, 1.0);
		LBVHNodeEncoded[i].AABB_MaxPos = glm::vec4(Node.AABB.MaxPos, 1.0);
	}
	return LBVHNodeEncoded;
}

void KH_Scene::Render()
{
	KH_Shader& Shader = KH_ExampleShaders::Instance().RayTracingShader4;
	KH_Framebuffer& Framebuffer = KH_Editor::Instance().GetCanvasFramebuffer();

	Framebuffer.Bind();

	Shader.Use();
	SetRayTracingParam();

	glBindVertexArray(KH_DefaultModels::Instance().Plane.VAO);
	glDrawElements(KH_DefaultModels::Instance().Plane.GetDrawMode(), static_cast<GLsizei>(KH_DefaultModels::Instance().Plane.GetIndicesSize()), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	Triangle_SSBO.Unbind();
	Material_SSBO.Unbind();
	LBVHNode_SSBO.Unbind();

	Framebuffer.Unbind();
}

void KH_Scene::AddTriangles(KH_Triangle& Triangle)
{
	Triangles.emplace_back(Triangle);
}

void KH_Scene::Clear()
{
	Triangles.clear();
}

KH_ExampleScenes::KH_ExampleScenes()
{
	InitExampleScene1();
	InitSingleTriangle();
}

void KH_ExampleScenes::InitExampleScene1()
{
	ExampleScene1.BVH.MaxBVHDepth = 9;
	ExampleScene1.BVH.MaxLeafTriangles = 1;

	ExampleScene1.BVH.BuildMode = KH_BVH_BUILD_MODE::SAH;

	KH_BRDFMaterial Material1;
	Material1.BaseColor = glm::vec3(1.0, 0.0, 0.0);

	KH_BRDFMaterial Material2;
	Material2.BaseColor = glm::vec3(0.0, 1.0, 0.0);

	glm::vec3 v0(-1.0f, 0.15f, -1.0f);
	glm::vec3 v1( 1.0f, 0.15f, -1.0f);
	glm::vec3 v2( 1.0f, 0.15f, 1.0f);
	glm::vec3 v3(-1.0f, 0.15f, 1.0f);

	KH_Triangle Triangle1(v0, v1, v2, 1);
	KH_Triangle Triangle2(v0, v2, v3, 1);

	ExampleScene1.Materials.push_back(Material1);
	ExampleScene1.Materials.push_back(Material2);

	ExampleScene1.LoadObj("Assert/Models/bunny.obj", 5.0, 0);
	ExampleScene1.AddTriangles(Triangle1);
	ExampleScene1.AddTriangles(Triangle2);
	ExampleScene1.BindAndBuild();
}

void KH_ExampleScenes::InitSingleTriangle()
{
	SingleTriangle.BVH.MaxBVHDepth = 11;
	SingleTriangle.BVH.MaxLeafTriangles = 1;

	SingleTriangle.BVH.BuildMode = KH_BVH_BUILD_MODE::SAH;

	KH_BRDFMaterial Material;
	Material.BaseColor = glm::vec3(1.0, 0.0, 0.0);

	SingleTriangle.Materials.push_back(Material);

	KH_Triangle Triangle(
		glm::vec3(-0.5, -0.5, 0.0),
		glm::vec3(0.5, -0.5, 0.0),
		glm::vec3(0.0, 0.5, 0.0)
	);

	SingleTriangle.AddTriangles(Triangle);
	SingleTriangle.BindAndBuild();
}
