#include "KH_Shape.h"
#include "Hit/KH_Ray.h"
#include "Utils/KH_DebugUtils.h"

#include "Editor/KH_Editor.h"
#include "KH_Model.h"

glm::quat KH_Object::EulerDegreesToQuatXYZ(const glm::vec3& degreesXYZ)
{
    const glm::vec3 r = glm::radians(degreesXYZ);

    const glm::quat qx = glm::angleAxis(r.x, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat qy = glm::angleAxis(r.y, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat qz = glm::angleAxis(r.z, glm::vec3(0.0f, 0.0f, 1.0f));

    // 对齐你原来的 X -> Y -> Z 旋转顺序
    return glm::normalize(qx * qy * qz);
}

glm::vec3 KH_Object::QuatToEulerDegreesXYZ(const glm::quat& q)
{
    return glm::degrees(glm::eulerAngles(glm::normalize(q)));
}

KH_InspectorEditResult KH_Object::DrawInspector()
{
    KH_InspectorEditResult result;

    ImGui::SeparatorText("Transform");
    ImGui::Indent(20.0f);

    glm::vec3 position = GetPosition();
    glm::vec3 rotation = GetRotation();
    glm::vec3 scale = GetScale();

    if (ImGui::DragFloat3("Position", &position.x, 0.01f))
    {
        SetPosition(position);
        result.bValueChanged = true;
        result.CommitType = KH_InspectorCommitType::RebuildBVH;
    }

    if (ImGui::DragFloat3("Rotation", &rotation.x, 0.5f))
    {
        SetRotation(rotation);
        result.bValueChanged = true;
        result.CommitType = KH_InspectorCommitType::RebuildBVH;
    }

    if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 1000.0f))
    {
        SetScale(scale);
        result.bValueChanged = true;
        result.CommitType = KH_InspectorCommitType::RebuildBVH;
    }

    ImGui::Spacing();
    ImGui::Unindent(20.0f);

    if (KH_Model* model = dynamic_cast<KH_Model*>(this))
    {
        KH_Editor& editor = KH_Editor::Instance();
        KH_GpuLBVHScene& scene = editor.Scene;

        const int selectedMeshID = editor.GetSelectedObjectMeshID();
        const auto& meshes = model->GetMeshes();

        ImGui::SeparatorText("Material");
        ImGui::Indent(20.0f);

        if (scene.Materials.empty())
        {
            ImGui::TextDisabled("No materials in scene");
        }
        else if (selectedMeshID < 0 || selectedMeshID >= static_cast<int>(meshes.size()))
        {
            ImGui::TextDisabled("No mesh selected");
        }
        else
        {
            int materialID = meshes[selectedMeshID].GetMaterialSlotID();
            materialID = std::clamp(materialID, 0, static_cast<int>(scene.Materials.size()) - 1);

            std::vector<std::string> labels;
            std::vector<const char*> items;
            labels.reserve(scene.Materials.size());
            items.reserve(scene.Materials.size());

            for (int i = 0; i < static_cast<int>(scene.Materials.size()); ++i)
            {
                labels.push_back("Material " + std::to_string(i));
            }

            for (auto& s : labels)
            {
                items.push_back(s.c_str());
            }

            if (ImGui::Combo("Material", &materialID, items.data(), static_cast<int>(items.size())))
            {
                materialID = std::clamp(materialID, 0, static_cast<int>(scene.Materials.size()) - 1);

                model->SetMeshMaterialSlotID(materialID, selectedMeshID);
                
                scene.UpdatePrimitiveSSBO();
                editor.RequestFrameReset();

                result.bValueChanged = true;
                result.CommitType = KH_InspectorCommitType::UpdateMaterial;
            }

            ImGui::TextDisabled("Selected Mesh ID: %d", selectedMeshID);
        }

        ImGui::Spacing();
        ImGui::Unindent(20.0f);
    }


    return result;
}

void KH_Object::SetPosition(const glm::vec3& InPosition)
{
    Position = InPosition;
    OnTransformChanged();
}

void KH_Object::SetPosition(float X, float Y, float Z)
{
    Position = glm::vec3(X, Y, Z);
    OnTransformChanged();
}

void KH_Object::SetScale(const glm::vec3& InScale)
{
    Scale = InScale;
    OnTransformChanged();
}

void KH_Object::SetScale(float X, float Y, float Z)
{
    Scale = glm::vec3(X, Y, Z);
    OnTransformChanged();
}

void KH_Object::SetUniformScale(float S)
{
    Scale = glm::vec3(S, S, S);
    OnTransformChanged();
}

void KH_Object::SetRotation(const glm::vec3& InRotation)
{
    Rotation = InRotation;
    RotationQuat = EulerDegreesToQuatXYZ(Rotation);
    OnTransformChanged();
}

void KH_Object::SetRotation(float Pitch, float Yaw, float Roll)
{
    Rotation = glm::vec3(Pitch, Yaw, Roll);
    RotationQuat = EulerDegreesToQuatXYZ(Rotation);
    OnTransformChanged();
}

void KH_Object::SetRotationQuat(const glm::quat& InRotationQuat)
{
    RotationQuat = glm::normalize(InRotationQuat);
    Rotation = QuatToEulerDegreesXYZ(RotationQuat); // 仅同步 UI 显示
    OnTransformChanged();
}

void KH_Object::SetTransform(const glm::vec3& InPosition, const glm::vec3& InRotation, const glm::vec3& InScale)
{
    Position = InPosition;
    Scale = InScale;
    Rotation = InRotation;
    RotationQuat = EulerDegreesToQuatXYZ(Rotation);
    OnTransformChanged();
}

void KH_Object::SetTransform(const glm::vec3& InPosition, const glm::quat& InRotationQuat, const glm::vec3& InScale)
{
    Position = InPosition;
    Scale = InScale;
    RotationQuat = glm::normalize(InRotationQuat);
    Rotation = QuatToEulerDegreesXYZ(RotationQuat); // 仅同步 UI 显示
    OnTransformChanged();
}

void KH_Object::Translate(const glm::vec3& Delta)
{
    Position += Delta;
    OnTransformChanged();
}

void KH_Object::Translate(float X, float Y, float Z)
{
    Position += glm::vec3(X, Y, Z);
    OnTransformChanged();
}

void KH_Object::Rotate(const glm::vec3& DeltaRotation)
{
    Rotation += DeltaRotation;
    RotationQuat = EulerDegreesToQuatXYZ(Rotation);
    OnTransformChanged();
}

void KH_Object::Rotate(float Pitch, float Yaw, float Roll)
{
    Rotation += glm::vec3(Pitch, Yaw, Roll);
    RotationQuat = EulerDegreesToQuatXYZ(Rotation);
    OnTransformChanged();
}

void KH_Object::AddScale(const glm::vec3& DeltaScale)
{
    Scale += DeltaScale;
    OnTransformChanged();
}

void KH_Object::AddScale(float X, float Y, float Z)
{
    Scale += glm::vec3(X, Y, Z);
    OnTransformChanged();
}

const glm::vec3& KH_Object::GetPosition() const
{
    return Position;
}

const glm::vec3& KH_Object::GetScale() const
{
    return Scale;
}

const glm::vec3& KH_Object::GetRotation() const
{
    return Rotation;
}

const glm::quat& KH_Object::GetRotationQuat() const
{
    return RotationQuat;
}

glm::mat4 KH_Object::GetModelMatrix() const
{
    return ModelMatrix;
}

glm::mat3 KH_Object::GetNormalMatrix() const
{
    return NormalMatrix;
}

void KH_Object::SetGizmoPivotLocal(const glm::vec3& InPivot)
{
    GizmoPivotLocal = InPivot;
}

const glm::vec3& KH_Object::GetGizmoPivotLocal() const
{
    return GizmoPivotLocal;
}

glm::vec3 KH_Object::GetGizmoPivotWorld() const
{
    return glm::vec3(ModelMatrix * glm::vec4(GizmoPivotLocal, 1.0f));
}

glm::mat4 KH_Object::GetGizmoMatrix() const
{
    return ModelMatrix * glm::translate(glm::mat4(1.0f), GizmoPivotLocal);
}

glm::mat3 KH_Object::GetNormalMatrix(glm::mat4 Model)
{
    return glm::transpose(glm::inverse(glm::mat3(Model)));
}

glm::mat4 KH_Object::UpdateModelMatrix()
{
    const glm::mat4 T = glm::translate(glm::mat4(1.0f), Position);
    const glm::mat4 R = glm::toMat4(glm::normalize(RotationQuat));
    const glm::mat4 S = glm::scale(glm::mat4(1.0f), Scale);

    ModelMatrix = T * R * S;
    return ModelMatrix;
}

glm::mat3 KH_Object::UpdateNormalMatrix()
{
    NormalMatrix = GetNormalMatrix(ModelMatrix);
    return NormalMatrix;
}

void KH_Object::OnTransformChanged()
{
    UpdateModelMatrix();
    UpdateNormalMatrix();
}

void KH_Hitable::CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const
{
    outCenters.push_back(glm::vec4(AABB.GetCenter(), 1.0f));
}

const KH_AABB& KH_Hitable::GetAABB() const
{
    return AABB;
}


void KH_Hitable::OnTransformChanged()
{
	KH_Object::OnTransformChanged();
    UpdateAABB();
}

glm::vec3 KH_Primitive::GetMinPos() const
{
    return AABB.MinPos;
}

glm::vec3 KH_Primitive::GetMaxPos() const
{
    return AABB.MaxPos;
}

glm::vec3 KH_Primitive::GetAABBCenter() const
{
    return AABB.GetCenter();
}

bool KH_Primitive::Cmpx(const KH_Primitive& p1, const KH_Primitive& p2)
{
    return p1.GetAABBCenter().x < p2.GetAABBCenter().x;
}

bool KH_Primitive::Cmpy(const KH_Primitive& p1, const KH_Primitive& p2)
{
    return p1.GetAABBCenter().y < p2.GetAABBCenter().y;
}

bool KH_Primitive::Cmpz(const KH_Primitive& p1, const KH_Primitive& p2)
{
    return p1.GetAABBCenter().z < p2.GetAABBCenter().z;
}

bool KH_Primitive::CmpxPtr(const std::unique_ptr<KH_Primitive>& p1, const std::unique_ptr<KH_Primitive>& p2)
{
    return Cmpx(*p1, *p2);
}

bool KH_Primitive::CmpyPtr(const std::unique_ptr<KH_Primitive>& p1, const std::unique_ptr<KH_Primitive>& p2)
{
    return Cmpy(*p1, *p2);
}

bool KH_Primitive::CmpzPtr(const std::unique_ptr<KH_Primitive>& p1, const std::unique_ptr<KH_Primitive>& p2)
{
    return Cmpz(*p1, *p2);
}


KH_Triangle::KH_Triangle()
{
    PrimitiveType = KH_PrimitiveType::Triangle;

    P1 = P2 = P3 = glm::vec3(0.0f, 0.0f, 0.0f);
    N1 = N2 = N3 = glm::vec3(0.0f, -1.0f, 0.0f);

    GizmoPivotLocal = (P1 + P2 + P3) * 0.33333333f;

    UpdateModelMatrix();
    UpdateNormalMatrix();
    UpdateAABB();
    UpdateOtherData();
}

KH_Triangle::KH_Triangle(glm::vec3 P1, glm::vec3 P2, glm::vec3 P3)
	: P1(P1), P2(P2), P3(P3)
{
    PrimitiveType = KH_PrimitiveType::Triangle;

    const glm::vec3 P1P2 = P2 - P1;
    const glm::vec3 P1P3 = P3 - P1;
    N1 = N2 = N3 = glm::normalize(glm::cross(P1P2, P1P3));

    GizmoPivotLocal = (P1 + P2 + P3) * 0.33333333f;

    UpdateModelMatrix();
    UpdateNormalMatrix();
    UpdateAABB();
    UpdateOtherData();
}

KH_Triangle::KH_Triangle(glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, glm::vec3 N1, glm::vec3 N2, glm::vec3 N3)
    : P1(P1), P2(P2), P3(P3), N1(N1), N2(N2), N3(N3)
{
    PrimitiveType = KH_PrimitiveType::Triangle;

    GizmoPivotLocal = (P1 + P2 + P3) * 0.33333333f;

    UpdateModelMatrix();
    UpdateNormalMatrix();
    UpdateAABB();
    UpdateOtherData();
}




KH_HitResult KH_Triangle::Hit(const KH_Ray& Ray) const
{
    // MT算法
    KH_TriangleWorldData Data = GetWorldData();

    KH_HitResult result;
    glm::vec3 edge1 = Data.P2 - Data.P1;
    glm::vec3 edge2 = Data.P3 - Data.P1;
    glm::vec3 pvec = glm::cross(Ray.Direction, edge2);
    float det = glm::dot(edge1, pvec);

    if (std::abs(det) < EPS) return result;

    float invDet = 1.0f / det;

    glm::vec3 tvec = Ray.Start - Data.P1;
    float u = glm::dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) return result;

    glm::vec3 qvec = glm::cross(tvec, edge1);
    float v = glm::dot(Ray.Direction, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return result;

    float t = glm::dot(edge2, qvec) * invDet;
    if (t < EPS) return result; 

    result.bIsHit = true;
    result.Distance = t;
    result.HitPoint = Ray.Start + t * Ray.Direction;

    float w1 = 1.0f - u - v;
    result.Normal = glm::normalize(w1 * Data.N1 + u * Data.N2 + v * Data.N3);

    return result;
}

uint32_t KH_Triangle::GetPrimitiveCount() const
{
    return 1;
}

void KH_Triangle::EncodePrimitives(std::vector<KH_PrimitiveEncoded>& outPrimitives) const
{
    KH_TriangleWorldData Data = GetWorldData();

    KH_PrimitiveEncoded primitive{};

    primitive.Triangle.P1 = glm::vec4(Data.P1, 1.0f);
    primitive.Triangle.P2 = glm::vec4(Data.P2, 1.0f);
    primitive.Triangle.P3 = glm::vec4(Data.P3, 1.0f);

    primitive.Triangle.N1 = glm::vec4(Data.N1, 0.0f);
    primitive.Triangle.N2 = glm::vec4(Data.N2, 0.0f);
    primitive.Triangle.N3 = glm::vec4(Data.N3, 0.0f);

    primitive.PrimitiveType = glm::ivec2(static_cast<int>(KH_PrimitiveType::Triangle), 0);
    primitive.MaterialSlotID = glm::ivec2(MaterialSlotID, 0);

    outPrimitives.push_back(primitive);
}

void KH_Triangle::CollectPrimitives(std::vector<KH_ScenePrimitive>& outPrimitives) const
{
    outPrimitives.emplace_back(KH_ScenePrimitive{
    std::make_unique<KH_Triangle>(*this)
        });
}

void KH_Triangle::CollectPrimitiveAABBCenters(std::vector<glm::vec4>& outCenters) const
{
    outCenters.push_back(glm::vec4(AABB.GetCenter(), 1.0f));
}

glm::vec3 KH_Triangle::GetCenterWS() const
{
    return CenterWS;
}

void KH_Triangle::UpdateAABB()
{
    KH_TriangleWorldData Data = GetWorldData();
    AABB.MinPos = glm::min(Data.P1, glm::min(Data.P2, Data.P3));
    AABB.MaxPos = glm::max(Data.P1, glm::max(Data.P2, Data.P3));
}

void KH_Triangle::UpdateOtherData()
{
    KH_TriangleWorldData Data = GetWorldData();
    CenterWS = (Data.P1 + Data.P2 + Data.P3) / 3.0f;
    NormalWS = glm::normalize(glm::cross(Data.P2 - Data.P1, Data.P3 - Data.P1));
}

void KH_Triangle::OnTransformChanged()
{
	KH_Primitive::OnTransformChanged();
    UpdateOtherData();
}

KH_Triangle::KH_TriangleHitInfo KH_Triangle::CheckHitInfo(glm::vec3 HitPoint)
{
    KH_TriangleWorldData Data = GetWorldData();

    const glm::vec3 P1P2 = Data.P2 - Data.P1;
    const glm::vec3 P1P3 = Data.P3 - Data.P1;
    const glm::vec3 P1P = HitPoint - Data.P1;

    float Dot00 = glm::dot(P1P3, P1P3);
    float Dot01 = glm::dot(P1P3, P1P2);
    float Dot02 = glm::dot(P1P3, P1P);
    float Dot11 = glm::dot(P1P2, P1P2);
    float Dot12 = glm::dot(P1P2, P1P);

    float Denom = Dot00 * Dot11 - Dot01 * Dot01;

    KH_TriangleHitInfo HitInfo;
    HitInfo.bIsHit = false;

    if (std::abs(Denom) < EPS)
        return HitInfo;

    float InvDenom = 1.0f / Denom;

    float u = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenom;
    float v = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenom;

    if (u >= 0 && v >= 0 && u + v <= 1) {
        HitInfo.bIsHit = true;
        HitInfo.w1 = 1 - u - v;
        HitInfo.w2 = u;
        HitInfo.w3 = v;
        return HitInfo;
    }
    return HitInfo;
}

KH_Triangle::KH_TriangleWorldData KH_Triangle::GetWorldData() const
{
    KH_TriangleWorldData data;
    data.P1 = glm::vec3(ModelMatrix * glm::vec4(P1, 1.0f));
    data.P2 = glm::vec3(ModelMatrix * glm::vec4(P2, 1.0f));
    data.P3 = glm::vec3(ModelMatrix * glm::vec4(P3, 1.0f));

    data.N1 = glm::normalize(NormalMatrix * N1);
    data.N2 = glm::normalize(NormalMatrix * N2);
    data.N3 = glm::normalize(NormalMatrix * N3);
    return data;
}
