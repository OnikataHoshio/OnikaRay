#include "KH_Mesh.h"

#include "Editor/KH_Editor.h"
#include "Pipeline/KH_Texture.h"
#include "Pipeline/KH_Shader.h"



KH_Mesh::KH_Mesh(std::vector<KH_Vertex>& Vertices, std::vector<unsigned int>& Indices, std::vector<KH_Texture>& Textures, GLenum DrawMode)
{
    Create(Vertices, Indices, Textures,DrawMode);
}

KH_Mesh::KH_Mesh(KH_Mesh&& other) noexcept
    : Vertices(std::move(other.Vertices)),
    Indices(std::move(other.Indices)),
    Textures(std::move(other.Textures)),
    VAO(other.VAO),
    VBO(other.VBO),
    EBO(other.EBO),
    DrawMode(other.DrawMode),
	LocalAABB(other.LocalAABB)
{
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
}


KH_Mesh& KH_Mesh::operator=(KH_Mesh&& other) noexcept
{
    if (this != &other)
    {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);

        Vertices = std::move(other.Vertices);
        Indices = std::move(other.Indices);
        Textures = std::move(other.Textures);

        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        DrawMode = other.DrawMode;
        LocalAABB = other.LocalAABB;

        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
    }
    return *this;
}

KH_Mesh::~KH_Mesh()
{
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
}

void KH_Mesh::Create(std::vector<KH_Vertex>& Vertices, std::vector<unsigned int>& Indices, std::vector<KH_Texture>& Textures, GLenum DrawMode)
{
    this->Vertices = Vertices;
    this->Indices = Indices;
    this->Textures = Textures;

    UpdateLocalAABB();
    SetDrawMode(DrawMode);
    SetupMesh();
}

void KH_Mesh::SetDrawMode(GLenum DrawMode)
{
    this->DrawMode = DrawMode;
}


void KH_Mesh::Render(KH_Shader& Shader)
{
    //Shader.Use();
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    for (unsigned int i = 0; i < Textures.size(); i++)
    {

        std::string number;
        std::string name =  std::string(magic_enum::enum_name(Textures[i].GetType()));
        if (Textures[i].GetType() == KH_TEXTURE_TYPE::DIFFUSE)
            number = std::to_string(diffuseNr++);
        else if (Textures[i].GetType() == KH_TEXTURE_TYPE::SPECULAR)
            number = std::to_string(specularNr++);


        glActiveTexture(GL_TEXTURE0 + i); 
        Shader.SetInt(("Material." + name + number).c_str(), i);
        glBindTexture(GL_TEXTURE_2D, Textures[i].GetID());
    }
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(VAO);
    glDrawElements(DrawMode, Indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

const unsigned int KH_Mesh::GetVAO() const
{
    return VAO;
}

GLsizei KH_Mesh::GetNumIndices() const
{
    return Indices.size();
}

uint32_t KH_Mesh::GetPrimitiveCount() const
{
	switch (DrawMode)
	{
	case GL_TRIANGLES:
        return Indices.size() / 3;
	default:
        return 0;
	}
}

void KH_Mesh::EncodePrimitives(std::vector<KH_PrimitiveEncoded>& outPrimitives,
	const glm::mat4& ModelMatrix, const glm::mat3& NormalMatrix) const
{
    if (DrawMode != GL_TRIANGLES)
        return;

    for (size_t i = 0; i + 2 < Indices.size(); i += 3)
    {
        const KH_Vertex& v0 = Vertices[Indices[i]];
        const KH_Vertex& v1 = Vertices[Indices[i + 1]];
        const KH_Vertex& v2 = Vertices[Indices[i + 2]];

        const glm::vec3 p0 = glm::vec3(ModelMatrix * glm::vec4(v0.Position, 1.0f));
        const glm::vec3 p1 = glm::vec3(ModelMatrix * glm::vec4(v1.Position, 1.0f));
        const glm::vec3 p2 = glm::vec3(ModelMatrix * glm::vec4(v2.Position, 1.0f));

        const glm::vec3 n0 = glm::normalize(NormalMatrix * v0.Normal);
        const glm::vec3 n1 = glm::normalize(NormalMatrix * v1.Normal);
        const glm::vec3 n2 = glm::normalize(NormalMatrix * v2.Normal);

        KH_PrimitiveEncoded primitive{};
        primitive.Triangle.P1 = glm::vec4(p0, 1.0f);
        primitive.Triangle.P2 = glm::vec4(p1, 1.0f);
        primitive.Triangle.P3 = glm::vec4(p2, 1.0f);

        primitive.Triangle.N1 = glm::vec4(n0, 0.0f);
        primitive.Triangle.N2 = glm::vec4(n1, 0.0f);
        primitive.Triangle.N3 = glm::vec4(n2, 0.0f);

        primitive.PrimitiveType = glm::ivec2(static_cast<int>(KH_PrimitiveType::Triangle), 0);
        primitive.MaterialSlotID = glm::ivec2(MaterialSlotID, 0);

        outPrimitives.push_back(primitive);
    }
}

void KH_Mesh::CollectPrimitives(std::vector<KH_ScenePrimitive>& outPrimitives,
	const glm::mat4& ModelMatrix, const glm::mat3& NormalMatrix) const
{
    if (DrawMode != GL_TRIANGLES)
        return;

    for (size_t i = 0; i + 2 < Indices.size(); i += 3)
    {
        const KH_Vertex& v0 = Vertices[Indices[i]];
        const KH_Vertex& v1 = Vertices[Indices[i + 1]];
        const KH_Vertex& v2 = Vertices[Indices[i + 2]];

        const glm::vec3 p0 = glm::vec3(ModelMatrix * glm::vec4(v0.Position, 1.0f));
        const glm::vec3 p1 = glm::vec3(ModelMatrix * glm::vec4(v1.Position, 1.0f));
        const glm::vec3 p2 = glm::vec3(ModelMatrix * glm::vec4(v2.Position, 1.0f));

        const glm::vec3 n0 = glm::normalize(NormalMatrix * v0.Normal);
        const glm::vec3 n1 = glm::normalize(NormalMatrix * v1.Normal);
        const glm::vec3 n2 = glm::normalize(NormalMatrix * v2.Normal);

        auto primitive = std::make_unique<KH_Triangle>(p0, p1, p2, n0, n1, n2);
        primitive->PrimitiveType = KH_PrimitiveType::Triangle;
        primitive->MaterialSlotID = MaterialSlotID;

        outPrimitives.emplace_back(KH_ScenePrimitive{
            std::move(primitive)
            });
    }
}

void KH_Mesh::CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters, const glm::mat4& ModelMatrix) const
{
    if (DrawMode != GL_TRIANGLES)
        return;

    for (size_t i = 0; i + 2 < Indices.size(); i += 3)
    {
        const KH_Vertex& v0 = Vertices[Indices[i]];
        const KH_Vertex& v1 = Vertices[Indices[i + 1]];
        const KH_Vertex& v2 = Vertices[Indices[i + 2]];

        const glm::vec3 p0 = glm::vec3(ModelMatrix * glm::vec4(v0.Position, 1.0f));
        const glm::vec3 p1 = glm::vec3(ModelMatrix * glm::vec4(v1.Position, 1.0f));
        const glm::vec3 p2 = glm::vec3(ModelMatrix * glm::vec4(v2.Position, 1.0f));

        glm::vec3 MinPos = glm::min(p0, glm::min(p1, p2));
        glm::vec3 MaxPos = glm::max(p0, glm::max(p1, p2));

        glm::vec3 Center = 0.5f * (MinPos + MaxPos);

        outCenters.emplace_back(glm::vec4(Center, 1.0f));
    }
}

const KH_AABB& KH_Mesh::GetLocalAABB() const
{
    return LocalAABB;
}

const std::vector<KH_Vertex>& KH_Mesh::GetVertices() const
{
    return Vertices;
}

const std::vector<unsigned int>& KH_Mesh::GetIndices() const
{
    return Indices;
}

GLenum KH_Mesh::GetDrawMode() const
{
    return DrawMode;
}

int KH_Mesh::GetMaterialSlotID() const
{
    return MaterialSlotID;
}

void KH_Mesh::SetMaterialSlotID(int InMaterialSlotID)
{
    MaterialSlotID = InMaterialSlotID;
}

KH_PickResult KH_Mesh::Pick(const KH_Ray& Ray, const glm::mat4& ModelMatrix, const glm::mat3& NormalMatrix) const
{
    KH_PickResult best;

    std::vector<KH_ScenePrimitive> primitives;
    primitives.reserve(GetPrimitiveCount());
    CollectPrimitives(primitives, ModelMatrix, NormalMatrix);

    for (auto& primitive : primitives)
    {
        KH_Ray localRay = Ray;
        KH_HitResult pick = primitive->Hit(localRay);

        if (pick.bIsHit && pick.Distance < best.Distance)
        {
            best.bIsHit = true;
            best.Distance = pick.Distance;
            best.MaterialSlotID = MaterialSlotID;
            best.ObjectMeshID = LocalMeshID;
            best.HitPoint = pick.HitPoint;
            best.Normal = pick.Normal;
        }
    }

    return best;
}



void KH_Mesh::SetupMesh()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(KH_Vertex), Vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, Indices.size() * sizeof(unsigned int),
        Indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(KH_Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(KH_Vertex), (void*)offsetof(KH_Vertex, Normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(KH_Vertex), (void*)offsetof(KH_Vertex, UV));

    glBindVertexArray(0);
}

void KH_Mesh::UpdateLocalAABB()
{
    if (Vertices.empty())
    {
        LocalAABB.MinPos = glm::vec3(0.0f);
        LocalAABB.MaxPos = glm::vec3(0.0f);
        return;
    }

    glm::vec3 minPos = Vertices[0].Position;
    glm::vec3 maxPos = Vertices[0].Position;

    for (const auto& v : Vertices)
    {
        minPos = glm::min(minPos, v.Position);
        maxPos = glm::max(maxPos, v.Position);
    }

    LocalAABB.MinPos = minPos;
    LocalAABB.MaxPos = maxPos;
}

