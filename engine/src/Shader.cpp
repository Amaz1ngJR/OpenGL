#include "Shader.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

namespace {
// 读取 GLSL 源码；路径错误时抛出异常，让主程序将错误显示在控制台。
std::string readFile(const char* path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error(std::string("Cannot open shader: ") + path);
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// 编译或链接失败时收集驱动日志，避免只得到一个无法定位的黑屏。
void check(unsigned int object, bool program, const char* path) {
    int success = 0;
    if (program) glGetProgramiv(object, GL_LINK_STATUS, &success);
    else glGetShaderiv(object, GL_COMPILE_STATUS, &success);
    if (success) return;
    char log[1024]{};
    if (program) glGetProgramInfoLog(object, sizeof(log), nullptr, log);
    else glGetShaderInfoLog(object, sizeof(log), nullptr, log);
    throw std::runtime_error(std::string("Shader error (") + path + "): " + log);
}
}

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    const std::string vertexSource = readFile(vertexPath);
    const std::string fragmentSource = readFile(fragmentPath);
    unsigned int vertex = 0, fragment = 0;
    try {
        const char* source = vertexSource.c_str();
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &source, nullptr);
        glCompileShader(vertex);
        check(vertex, false, vertexPath);
        source = fragmentSource.c_str();
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &source, nullptr);
        glCompileShader(fragment);
        check(fragment, false, fragmentPath);
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        check(ID, true, vertexPath);
    } catch (...) {
        // 构造函数中途异常也释放已创建的 GL 对象，避免加载错误导致泄漏。
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        if (ID) glDeleteProgram(ID);
        throw;
    }
    // 链接完成后 program 已包含着色器，两个临时 shader 可以删除。
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

// Shader 对象由场景拥有，离开场景时自动释放 program。
Shader::~Shader() { if (ID) glDeleteProgram(ID); }
void Shader::use() const { glUseProgram(ID); }
void Shader::setInt(const std::string& name, int value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }
void Shader::setFloat(const std::string& name, float value) const { glUniform1f(glGetUniformLocation(ID, name.c_str()), value); }
void Shader::setVec3(const std::string& name, const glm::vec3& value) const { glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value)); }
void Shader::setMat4(const std::string& name, const glm::mat4& value) const { glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(value)); }
