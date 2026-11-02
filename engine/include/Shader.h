#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

// GLSL 程序的 RAII 封装：构造时编译/链接，析构时释放 OpenGL program。
class Shader {
public:
    unsigned int ID = 0;
    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
};
