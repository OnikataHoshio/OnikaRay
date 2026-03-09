#include "KH_Shader.h"
#include "Utils/KH_DebugUtils.h"

KH_Shader::KH_Shader(const char* vertexPath, const char* fragmentPath)
{
    Create(vertexPath, fragmentPath);
}

KH_Shader::KH_Shader(const char* computePath)
{
    Create(computePath);
}

KH_Shader::~KH_Shader()
{
    glDeleteProgram(ID);
}

void KH_Shader::Create(const char* vertexPath, const char* fragmentPath)
{
    if (ID != 0)
        return;

    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;

        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex, fragment;

    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    CheckCompileErrors(vertex, "VERTEX");

    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    CheckCompileErrors(fragment, "FRAGMENT");

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    CheckCompileErrors(ID, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void KH_Shader::Create(const char* computePath)
{
    if (ID != 0)
        return;

    std::string computeCode;
    std::ifstream cShaderFile;

    cShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        cShaderFile.open(computePath);
        std::stringstream cShaderStream;
        cShaderStream << cShaderFile.rdbuf();
        cShaderFile.close();
        computeCode = cShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::COMPUTE_SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
    }

    const char* cShaderCode = computeCode.c_str();

    unsigned int compute = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute, 1, &cShaderCode, NULL);
    glCompileShader(compute);
    CheckCompileErrors(compute, "COMPUTE");

    ID = glCreateProgram();
    glAttachShader(ID, compute);
    glLinkProgram(ID);
    CheckCompileErrors(ID, "PROGRAM");

    glDeleteShader(compute);
}

void KH_Shader::Use() const
{
	glUseProgram(ID);
}


void KH_Shader::SetInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void KH_Shader::SetUint(const std::string& name, uint32_t value) const
{
    glUniform1ui(glGetUniformLocation(ID, name.c_str()), value);
}

void KH_Shader::SetFloat(const std::string& name, float value) const
{
    unsigned int Location = glGetUniformLocation(ID, name.c_str());
    glUniform1f(Location, value);
}

void KH_Shader::SetMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void KH_Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void KH_Shader::CheckCompileErrors(unsigned int shader, std::string type)
{
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: "
        	<< type << "\n" << infoLog
        	<< "\n -- --------------------------------------------------- -- "
        	<< std::endl;
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: "
        	<< type << "\n" << infoLog
        	<< "\n -- --------------------------------------------------- -- "
        	<< std::endl;
        }
    }
}

KH_ExampleShaders::KH_ExampleShaders()
{
    InitShaders();
}

void KH_ExampleShaders::InitShaders()
{
    TestShader.Create("Assert/Shaders/test.vert", "Assert/Shaders/test.frag");
    PrintShaderLoadMessage("TestShader");
    AABBShader.Create("Assert/Shaders/DrawAABBs.vert", "Assert/Shaders/DrawAABBs.frag");
    PrintShaderLoadMessage("AABBShader");
    TestCanvasShader.Create("Assert/Shaders/DefaultCanvas.vert", "Assert/Shaders/DefaultCanvas.frag");
    PrintShaderLoadMessage("TestCanvasShader");
    RayTracingShader1.Create("Assert/Shaders/DefaultCanvas.vert", "Assert/Shaders/RayTracingV1.frag");
    PrintShaderLoadMessage("RayTracingShaderV1");
    RayTracingShader2.Create("Assert/Shaders/DefaultCanvas.vert", "Assert/Shaders/RayTracingV2.frag");
    PrintShaderLoadMessage("RayTracingShaderV2");
    RayTracingShader3.Create("Assert/Shaders/DefaultCanvas.vert", "Assert/Shaders/RayTracingV3.frag");
    PrintShaderLoadMessage("RayTracingShaderV3");
    RayTracingShader4.Create("Assert/Shaders/DefaultCanvas.vert", "Assert/Shaders/RayTracingV4.frag");
    PrintShaderLoadMessage("RayTracingShaderV4");
}

void KH_ExampleShaders::PrintShaderLoadMessage(std::string ShaderName)
{
    std::string DebugMessage = std::format("Shader ({}) has been loaded.", ShaderName);
    LOG_D(DebugMessage);
}
