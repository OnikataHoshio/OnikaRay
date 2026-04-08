#include "KH_Model.h"

#include "Hit/KH_Ray.h"
#include "Pipeline/KH_Shader.h"
#include "Utils/KH_DebugUtils.h"


KH_Model::KH_Model(const std::string& path)
{
    LoadModel(path);
}

KH_Model::KH_Model(KH_Model&& other) noexcept
    : Meshes(std::move(other.Meshes)),
    Directory(std::move(other.Directory)),
    SourcePath(std::move(other.SourcePath)),
    SourceType(other.SourceType),
    BuiltinType(other.BuiltinType),
    BuiltinSize(other.BuiltinSize)
{
    AABB = other.AABB;
}

KH_Model& KH_Model::operator=(KH_Model&& other) noexcept
{
    if (this != &other)
    {
        Meshes = std::move(other.Meshes);
        Directory = std::move(other.Directory);
        SourcePath = std::move(other.SourcePath);
        AABB = other.AABB;
        SourceType = other.SourceType;
        BuiltinType = other.BuiltinType;
        BuiltinSize = other.BuiltinSize;
    }
    return *this;
}

void KH_Model::SetSourceAsInline()
{
    SourceType = KH_ModelSourceType::Inline;
    BuiltinType = KH_BuiltinModelType::None;
    BuiltinSize = 1.0f;
    SourcePath.clear();
    Directory.clear();
}

void KH_Model::SetMeshMaterialSlotID(int MaterialSlotID)
{
    for (auto& mesh : Meshes)
    {
        mesh.MaterialSlotID = MaterialSlotID;
    }
}

void KH_Model::SetMeshMaterialSlotID(int MaterialSlotID, int MeshID)
{
    if (MaterialSlotID < 0)
        MaterialSlotID = 0;

    if (MeshID < 0 || MeshID >= static_cast<int>(Meshes.size()))
        return;

    Meshes[MeshID].MaterialSlotID = MaterialSlotID;
}


void KH_Model::LoadModel(const std::string& path)
{
    SourceType = KH_ModelSourceType::Asset;
    BuiltinType = KH_BuiltinModelType::None;
    BuiltinSize = 1.0f;
    SourcePath = path;

    Assimp::Importer import;

	import.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_NORMALS);

	import.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);

    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_RemoveComponent |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace;

    const aiScene* scene = import.ReadFile(path, flags);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        LOG_E(std::format("ASSIMP::{}", import.GetErrorString()));
        return;
    }

    Meshes.clear();
    AABB.Reset();

    Directory = std::filesystem::path(path).parent_path().string();
    ProcessNode(scene->mRootNode, scene);
    UpdateAABB();
    UpdateGizmoPivotLocal();

    for (int i = 0; i < Meshes.size(); i++)
    {
        Meshes[i].LocalMeshID = i;
    }

}

KH_PickResult KH_Model::Pick(const KH_Ray& Ray) const
{
    KH_PickResult best;

    const glm::mat4 model = GetModelMatrix();
    const glm::mat3 normal = GetNormalMatrix();

    for (auto& mesh : Meshes)
    {
        KH_Ray localRay = Ray;
        KH_PickResult pick = mesh.Pick(localRay, model, normal);
        if (pick.bIsHit && pick.Distance < best.Distance)
        {
            best = pick;
        }
    }

    return best;
}

//void KH_Model::AddMesh(const KH_Mesh& mesh)
//{
//    Meshes.push_back(std::move(mesh));
//}

void KH_Model::AddMesh(KH_Mesh&& mesh)
{
    Meshes.push_back(std::move(mesh));
    SetSourceAsInline();
    UpdateAABB();
    UpdateGizmoPivotLocal();
    Meshes.back().LocalMeshID = Meshes.size() - 1;
}


KH_Model KH_Model::CreateBuiltin(KH_BuiltinModelType type, float size)
{
    KH_Model model;

    switch (type)
    {
    case KH_BuiltinModelType::FullscreenQuad:
        model.AddMesh(KH_PrimitiveFactory::CreateFullscreenQuadMesh(size));
        break;
    case KH_BuiltinModelType::Plane:
        model.AddMesh(KH_PrimitiveFactory::CreatePlaneMesh(size));
        break;
    case KH_BuiltinModelType::Cube:
        model.AddMesh(KH_PrimitiveFactory::CreateCubeMesh(size));
        break;
    case KH_BuiltinModelType::EmptyCube:
        model.AddMesh(KH_PrimitiveFactory::CreateEmptyCubeMesh(size));
        break;
    default:
        break;
    }

    model.SourceType = KH_ModelSourceType::Builtin;
    model.BuiltinType = type;
    model.BuiltinSize = size;
    model.SourcePath.clear();
    model.Directory.clear();

    return model;
}

KH_HitResult KH_Model::Hit(const KH_Ray& Ray) const
{
    KH_HitResult best;

    std::vector<KH_ScenePrimitive> primitives;
    primitives.reserve(GetPrimitiveCount());
    CollectPrimitives(primitives);

    for (auto& primitive : primitives)
    {
        KH_Ray localRay = Ray;
        KH_HitResult hit = primitive->Hit(localRay);

        if (hit.bIsHit && hit.Distance < best.Distance)
        {
            best = hit;
        }
    }

    return best;
}


uint32_t KH_Model::GetPrimitiveCount() const
{
    uint32_t NumPrimitive = 0;
    for (const auto& mesh : Meshes)
        NumPrimitive += mesh.GetPrimitiveCount();
    return NumPrimitive;
}

void KH_Model::EncodePrimitives(std::vector<KH_PrimitiveEncoded>& outPrimitives) const
{
    const glm::mat4 model = GetModelMatrix();
    const glm::mat3 normal = GetNormalMatrix();

    for (const auto& mesh : Meshes)
    {
        mesh.EncodePrimitives(outPrimitives, model, normal);
    }
}

void KH_Model::CollectPrimitives(std::vector<KH_ScenePrimitive>& outPrimitives) const
{
    const glm::mat4 model = GetModelMatrix();
    const glm::mat3 normal = GetNormalMatrix();

    for (const auto& mesh : Meshes)
    {
        mesh.CollectPrimitives(outPrimitives, model, normal);
    }
}

void KH_Model::CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const
{
    const glm::mat4 model = GetModelMatrix();
    for (const auto& mesh : Meshes)
    {
        mesh.CollectPrimitiveAABBCenters(outCenters, ModelMatrix);
    }
}

const KH_AABB& KH_Model::GetAABB() const
{
    return AABB;
}


void KH_Model::Render(KH_Shader& Shader)
{
    Shader.SetMat4("model", GetModelMatrix());
	for (unsigned int i = 0; i < Meshes.size(); i++)
		Meshes[i].Render(Shader);
}


void KH_Model::ProcessNode(aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        Meshes.push_back(ProcessMesh(mesh, scene));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        ProcessNode(node->mChildren[i], scene);
    }
}

KH_Mesh KH_Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
		std::vector<KH_Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<KH_Texture> textures;

        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            KH_Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            // normals
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            // texture coordinates
            if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x; 
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.UV = vec;
                // tangent
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                // bitangent
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
            else
                vertex.UV = glm::vec2(0.0f, 0.0f);

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);        
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    
        // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
        // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
        // Same applies to other texture as the following list summarizes:
        // diffuse: texture_diffuseN
        // specular: texture_specularN
        // normal: texture_normalN

        // 1. diffuse maps
        std::vector<KH_Texture> diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE);
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. specular maps
        std::vector<KH_Texture> specularMaps = LoadMaterialTextures(material, aiTextureType_SPECULAR);
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps
        std::vector<KH_Texture> normalMaps = LoadMaterialTextures(material, aiTextureType_HEIGHT);
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height maps
        std::vector<KH_Texture> heightMaps = LoadMaterialTextures(material, aiTextureType_AMBIENT);
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        
        // return a mesh object created from the extracted mesh data
        return KH_Mesh(vertices, indices, textures);
}

std::vector<KH_Texture> KH_Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type)
{
    std::vector<KH_Texture> textures;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);

        std::filesystem::path fullPath = std::filesystem::path(Directory) / str.C_Str();

        KH_TEXTURE_TYPE khType = KH_TEXTURE_TYPE::DEFAULT;
        switch (type)
        {
        case aiTextureType_DIFFUSE:
            khType = KH_TEXTURE_TYPE::DIFFUSE;
            break;
        case aiTextureType_SPECULAR:
            khType = KH_TEXTURE_TYPE::SPECULAR;
            break;
        case aiTextureType_HEIGHT:
            khType = KH_TEXTURE_TYPE::NORMAL;
            break;
        case aiTextureType_AMBIENT:
            khType = KH_TEXTURE_TYPE::HEIGHT;
            break;
        default:
            khType = KH_TEXTURE_TYPE::DEFAULT;
            break;
        }

        KH_Texture tex = KH_TextureManager::Instance().LoadTexture(fullPath.string(), true, khType, true);
        if (tex.IsValid())
        {
            textures.push_back(tex);
        }
    }

    return textures;
}

void KH_Model::UpdateAABB()
{
    AABB.Reset();
    const glm::mat4 model = GetModelMatrix();
    for (const auto& mesh : Meshes)
    {
        AABB.Merge(mesh.GetLocalAABB());
    }
    AABB.MinPos = glm::vec3(model * glm::vec4(AABB.MinPos, 1.0f));
    AABB.MaxPos = glm::vec3(model * glm::vec4(AABB.MaxPos, 1.0f));
}

void KH_Model::UpdateGizmoPivotLocal()
{
    glm::vec3 localMin(FLT_MAX);
    glm::vec3 localMax(-FLT_MAX);

    for (const auto& mesh : Meshes)
    {
	    for (const auto& v : mesh.Vertices)
	    {
            localMin = glm::min(localMin, v.Position);
            localMax = glm::max(localMax, v.Position);
	    }
    }

    glm::vec3 localCenter = (localMin + localMax) * 0.5f;

    GizmoPivotLocal = localCenter;
}

void KH_Model::OnTransformChanged()
{
	KH_Object::OnTransformChanged();
    UpdateAABB();
}

KH_Mesh KH_PrimitiveFactory::CreateFullscreenQuadMesh(float size)
{
    const float halfSize = size * 0.5f;

    std::vector<KH_Vertex> vertices(4);

    // 顶点 0：左下
    vertices[0].Position = glm::vec3(-halfSize, -halfSize, 0.0f);
    vertices[0].Normal = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[0].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[0].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[0].UV = glm::vec2(0.0f, 0.0f);

    // 顶点 1：右下
    vertices[1].Position = glm::vec3(halfSize, -halfSize, 0.0f);
    vertices[1].Normal = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[1].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[1].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[1].UV = glm::vec2(1.0f, 0.0f);

    // 顶点 2：右上
    vertices[2].Position = glm::vec3(halfSize, halfSize, 0.0f);
    vertices[2].Normal = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[2].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[2].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[2].UV = glm::vec2(1.0f, 1.0f);

    // 顶点 3：左上
    vertices[3].Position = glm::vec3(-halfSize, halfSize, 0.0f);
    vertices[3].Normal = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[3].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[3].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[3].UV = glm::vec2(0.0f, 1.0f);

    std::vector<unsigned int> indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    std::vector<KH_Texture> textures;

    return KH_Mesh(vertices, indices, textures);
}

KH_Mesh KH_PrimitiveFactory::CreatePlaneMesh(float size)
{
    const float halfSize = size * 0.5f;

    std::vector<KH_Vertex> vertices(4);

    // 顶点 0：左下
    vertices[0].Position = glm::vec3(-halfSize, 0.0f, -halfSize);
    vertices[0].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[0].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[0].Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[0].UV = glm::vec2(0.0f, 0.0f);

    // 顶点 1：右下
    vertices[1].Position = glm::vec3(halfSize, 0.0f, -halfSize);
    vertices[1].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[1].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[1].Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[1].UV = glm::vec2(1.0f, 0.0f);

    // 顶点 2：右上
    vertices[2].Position = glm::vec3(halfSize, 0.0f, halfSize);
    vertices[2].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[2].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[2].Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[2].UV = glm::vec2(1.0f, 1.0f);

    // 顶点 3：左上
    vertices[3].Position = glm::vec3(-halfSize, 0.0f, halfSize);
    vertices[3].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    vertices[3].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    vertices[3].Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices[3].UV = glm::vec2(0.0f, 1.0f);

    std::vector<unsigned int> indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    std::vector<KH_Texture> textures;
    return KH_Mesh(vertices, indices, textures);
}


KH_Mesh KH_PrimitiveFactory::CreateCubeMesh(float size)
{
    const float h = size * 0.5f;

    std::vector<KH_Vertex> vertices(24);

    // Front (+Z)
    vertices[0].Position = glm::vec3(-h, -h, h);
    vertices[1].Position = glm::vec3(h, -h, h);
    vertices[2].Position = glm::vec3(h, h, h);
    vertices[3].Position = glm::vec3(-h, h, h);
    for (int i = 0; i < 4; ++i)
    {
        vertices[i].Normal = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices[i].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vertices[i].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    vertices[0].UV = glm::vec2(0.0f, 0.0f);
    vertices[1].UV = glm::vec2(1.0f, 0.0f);
    vertices[2].UV = glm::vec2(1.0f, 1.0f);
    vertices[3].UV = glm::vec2(0.0f, 1.0f);

    // Back (-Z)
    vertices[4].Position = glm::vec3(h, -h, -h);
    vertices[5].Position = glm::vec3(-h, -h, -h);
    vertices[6].Position = glm::vec3(-h, h, -h);
    vertices[7].Position = glm::vec3(h, h, -h);
    for (int i = 4; i < 8; ++i)
    {
        vertices[i].Normal = glm::vec3(0.0f, 0.0f, -1.0f);
        vertices[i].Tangent = glm::vec3(-1.0f, 0.0f, 0.0f);
        vertices[i].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    vertices[4].UV = glm::vec2(0.0f, 0.0f);
    vertices[5].UV = glm::vec2(1.0f, 0.0f);
    vertices[6].UV = glm::vec2(1.0f, 1.0f);
    vertices[7].UV = glm::vec2(0.0f, 1.0f);

    // Left (-X)
    vertices[8].Position = glm::vec3(-h, -h, -h);
    vertices[9].Position = glm::vec3(-h, -h, h);
    vertices[10].Position = glm::vec3(-h, h, h);
    vertices[11].Position = glm::vec3(-h, h, -h);
    for (int i = 8; i < 12; ++i)
    {
        vertices[i].Normal = glm::vec3(-1.0f, 0.0f, 0.0f);
        vertices[i].Tangent = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices[i].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    vertices[8].UV = glm::vec2(0.0f, 0.0f);
    vertices[9].UV = glm::vec2(1.0f, 0.0f);
    vertices[10].UV = glm::vec2(1.0f, 1.0f);
    vertices[11].UV = glm::vec2(0.0f, 1.0f);

    // Right (+X)
    vertices[12].Position = glm::vec3(h, -h, h);
    vertices[13].Position = glm::vec3(h, -h, -h);
    vertices[14].Position = glm::vec3(h, h, -h);
    vertices[15].Position = glm::vec3(h, h, h);
    for (int i = 12; i < 16; ++i)
    {
        vertices[i].Normal = glm::vec3(1.0f, 0.0f, 0.0f);
        vertices[i].Tangent = glm::vec3(0.0f, 0.0f, -1.0f);
        vertices[i].Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    vertices[12].UV = glm::vec2(0.0f, 0.0f);
    vertices[13].UV = glm::vec2(1.0f, 0.0f);
    vertices[14].UV = glm::vec2(1.0f, 1.0f);
    vertices[15].UV = glm::vec2(0.0f, 1.0f);

    // Top (+Y)
    vertices[16].Position = glm::vec3(-h, h, h);
    vertices[17].Position = glm::vec3(h, h, h);
    vertices[18].Position = glm::vec3(h, h, -h);
    vertices[19].Position = glm::vec3(-h, h, -h);
    for (int i = 16; i < 20; ++i)
    {
        vertices[i].Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        vertices[i].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vertices[i].Bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    vertices[16].UV = glm::vec2(0.0f, 0.0f);
    vertices[17].UV = glm::vec2(1.0f, 0.0f);
    vertices[18].UV = glm::vec2(1.0f, 1.0f);
    vertices[19].UV = glm::vec2(0.0f, 1.0f);

    // Bottom (-Y)
    vertices[20].Position = glm::vec3(-h, -h, -h);
    vertices[21].Position = glm::vec3(h, -h, -h);
    vertices[22].Position = glm::vec3(h, -h, h);
    vertices[23].Position = glm::vec3(-h, -h, h);
    for (int i = 20; i < 24; ++i)
    {
        vertices[i].Normal = glm::vec3(0.0f, -1.0f, 0.0f);
        vertices[i].Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vertices[i].Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    vertices[20].UV = glm::vec2(0.0f, 0.0f);
    vertices[21].UV = glm::vec2(1.0f, 0.0f);
    vertices[22].UV = glm::vec2(1.0f, 1.0f);
    vertices[23].UV = glm::vec2(0.0f, 1.0f);

    std::vector<unsigned int> indices =
    {
        0, 1, 2,  0, 2, 3,        // Front
        4, 5, 6,  4, 6, 7,        // Back
        8, 9, 10, 8, 10, 11,      // Left
        12, 13, 14, 12, 14, 15,   // Right
        16, 17, 18, 16, 18, 19,   // Top
        20, 21, 22, 20, 22, 23    // Bottom
    };

    std::vector<KH_Texture> textures;
    return KH_Mesh(vertices, indices, textures);
}


KH_Mesh KH_PrimitiveFactory::CreateEmptyCubeMesh(float size)
{
    const float h = size * 0.5f;

    std::vector<KH_Vertex> vertices(8);

    // 前面 4 个点 (+Z)
    vertices[0].Position = glm::vec3(-h, -h, h); // 左下前
    vertices[1].Position = glm::vec3(h, -h, h); // 右下前
    vertices[2].Position = glm::vec3(h, h, h); // 右上前
    vertices[3].Position = glm::vec3(-h, h, h); // 左上前

    // 后面 4 个点 (-Z)
    vertices[4].Position = glm::vec3(-h, -h, -h); // 左下后
    vertices[5].Position = glm::vec3(h, -h, -h); // 右下后
    vertices[6].Position = glm::vec3(h, h, -h); // 右上后
    vertices[7].Position = glm::vec3(-h, h, -h); // 左上后

    // 线框通常不需要法线/切线，但为了避免未初始化，补默认值
    for (auto& v : vertices)
    {
        v.Normal = glm::vec3(0.0f, 0.0f, 0.0f);
        v.UV = glm::vec2(0.0f, 0.0f);
        v.Tangent = glm::vec3(0.0f, 0.0f, 0.0f);
        v.Bitangent = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    std::vector<unsigned int> indices =
    {
        // 前面框
        0, 1,  1, 2,  2, 3,  3, 0,

        // 后面框
        4, 5,  5, 6,  6, 7,  7, 4,

        // 前后连接
        0, 4,  1, 5,  2, 6,  3, 7
    };

    std::vector<KH_Texture> textures;

    return KH_Mesh(vertices, indices, textures, GL_LINES);
}

KH_Model KH_PrimitiveFactory::CreateFullscreenQuad(float size)
{
    return KH_Model::CreateBuiltin(KH_BuiltinModelType::FullscreenQuad, size);
}

KH_Model KH_PrimitiveFactory::CreatePlane(float size)
{
    return KH_Model::CreateBuiltin(KH_BuiltinModelType::Plane, size);
}

KH_Model KH_PrimitiveFactory::CreateCube(float size)
{
    return KH_Model::CreateBuiltin(KH_BuiltinModelType::Cube, size);
}

KH_Model KH_PrimitiveFactory::CreateEmptyCube(float size)
{
    return KH_Model::CreateBuiltin(KH_BuiltinModelType::EmptyCube, size);
}

KH_DefaultModels::KH_DefaultModels()
{
    InitCube();
    InitEmptyCube();
    InitFullscreenQuad();
    InitPlane();
    InitBunny();
}

void KH_DefaultModels::InitCube()
{
    Cube = KH_PrimitiveFactory::CreateCubeMesh();
}

void KH_DefaultModels::InitEmptyCube()
{
    EmptyCube = KH_PrimitiveFactory::CreateEmptyCubeMesh(2.0);
}

void KH_DefaultModels::InitFullscreenQuad()
{
    FullscreenQuad = KH_PrimitiveFactory::CreateFullscreenQuadMesh(2.0);
}

void KH_DefaultModels::InitPlane()
{
    Plane = KH_PrimitiveFactory::CreatePlane();
}

void KH_DefaultModels::InitBunny()
{
    Bunny.LoadModel("Assert/Models/bunny.obj");
}


