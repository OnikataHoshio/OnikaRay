#include "KH_Camera.h"

#include "KH_Editor.h"
#include "Hit/KH_Ray.h"

#include "Utils/KH_RandomUtils.h"

KH_Camera::KH_Camera(uint32_t width, uint32_t height, glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    :MovementSpeed(2.5f),
    MouseSensitivity(0.1f),
    Fovy(45.0f),
    Front(0.0f, 0.0f, -1.0f)
{
    Width = width;
    Height = height;
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;

    UpdateAspect();

    UpdateCameraVectors();
}

glm::mat4 KH_Camera::GetViewMatrix() const
{
    return glm::lookAt(Position, Position + Front, Up);
}

glm::mat4 KH_Camera::GetProjMatrix() const
{
    return glm::perspective(glm::radians(Fovy), Aspect, NearPlane, FarPlane);
}

KH_Ray KH_Camera::GetRay(int i, int j, int width, int height) const
{
    KH_Ray Ray;
    Ray.Direction = GetRayDirection(i, j, width, height);
    Ray.Start = Position;
    return Ray;
}

KH_Ray KH_Camera::GetRay(int i, int j) const
{
    KH_Ray Ray;
    Ray.Direction = GetRayDirection(i, j, Width, Height);
    Ray.Start = Position;
    return Ray;
}

KH_Ray KH_Camera::GetRay(float u, float v) const
{
    KH_Ray Ray;
    Ray.Direction = GetRayDirection(u, v);
    Ray.Start = Position;
    return Ray;
}

KH_Ray KH_Camera::GetRay(glm::vec2 ndc) const
{
    KH_Ray Ray;
    Ray.Direction = GetRayDirection(ndc.x, ndc.y);
    Ray.Start = Position;
    return Ray;
}

void KH_Camera::UpdateAspect()
{
    Aspect = static_cast<float>(Width) / static_cast<float>(Height);
}


void KH_Camera::ProcessKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = MovementSpeed * deltaTime;

    if (direction == CameraMovement::Forward)
        Position += Front * velocity;
    if (direction == CameraMovement::Backward)
        Position -= Front * velocity;
    if (direction == CameraMovement::Left)
        Position -= Right * velocity;
    if (direction == CameraMovement::Right)
        Position += Right * velocity;
    if (direction == CameraMovement::Up)
        Position += Up * velocity;
    if (direction == CameraMovement::Down)
        Position -= Up * velocity;

    KH_Editor::Instance().RequestFrameReset();
}

void KH_Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    UpdateCameraVectors();

    KH_Editor::Instance().RequestFrameReset();
}

void KH_Camera::ProcessMouseScroll(float yoffset)
{
    MovementSpeed += yoffset * SpeedSensitivity;

    if (MovementSpeed < 0.1f)
        MovementSpeed = 0.1f;

    if (MovementSpeed > 20.0f)
        MovementSpeed = 20.0f;

    KH_Editor::Instance().RequestFrameReset();
}

void KH_Camera::UpdateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up = glm::normalize(glm::cross(Right, Front));
}


glm::vec3 KH_Camera::GetRayDirection(int i, int j, int Width, int Height) const
{
    float offsetU = KH_RandomUtils::Instance().RandomFloat();
    float offsetV = KH_RandomUtils::Instance().RandomFloat();

    float u = ((static_cast<float>(i) + offsetU) / Width) * 2.0f - 1.0f;
    float v = 1.0f - ((static_cast<float>(j) + offsetV) / Height) * 2.0f;

    return GetRayDirection(u, v);
}

glm::vec3 KH_Camera::GetRayDirection(float u, float v) const
{
    float scale = tan(glm::radians(Fovy) * 0.5f);

    glm::vec3 DirWorldSpace = (u * Aspect * scale) * Right +
        (v * scale) * Up +
        Front;

    return glm::normalize(DirWorldSpace);
}


