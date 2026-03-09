#pragma once

#include "KH_Common.h"

class KH_Shader {
public:
    unsigned int ID = 0;

    KH_Shader() = default;
    KH_Shader(const char* vertexPath, const char* fragmentPath);
    KH_Shader(const char* computePath);
    ~KH_Shader();

    void Create(const char* vertexPath, const char* fragmentPath);
    void Create(const char* computePath);
    void Use() const;

    void SetInt(const std::string& name, int value) const;
    void SetUint(const std::string& name, uint32_t value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetMat4(const std::string& name, const glm::mat4& mat) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;

private:
    static void CheckCompileErrors(unsigned int shader, std::string type);
};

class KH_ExampleShaders : public KH_Singleton<KH_ExampleShaders>
{
    friend class KH_Singleton<KH_ExampleShaders>;
private:
    KH_ExampleShaders();
    virtual ~KH_ExampleShaders() override = default;

    void InitShaders();

    void PrintShaderLoadMessage(std::string ShaderName);
public:
    KH_Shader TestShader;
    KH_Shader AABBShader;
    KH_Shader TestCanvasShader;
    KH_Shader RayTracingShader1;
    KH_Shader RayTracingShader2;
    KH_Shader RayTracingShader3;
    KH_Shader RayTracingShader4;
};