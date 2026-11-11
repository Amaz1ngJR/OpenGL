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
    // program ID 由此对象独占；禁止拷贝，避免多个析构函数重复删除同一个 GPU 对象。
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    // uniform 在一次绘制中对全部顶点/片元共享；调用 set* 前必须先 use() 激活本程序。
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
};
