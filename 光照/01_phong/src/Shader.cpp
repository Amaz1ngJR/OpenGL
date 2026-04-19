#include "Shader.h"

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    // 从文件路径中加载顶点/片段源代码
    std::string vertexCode, fragmentCode;// 用于存储读取到的着色器源码字符串
    std::ifstream vShaderFile, fShaderFile;// 用于读取文件的输入文件流对象

    // 确保ifstream 对象可以引发异常:
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        // 打开文件
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        // 将文件的缓冲区内容读入流
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        // 读完关闭文件
        vShaderFile.close();
        fShaderFile.close();
        // 将流转换成字符串
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }
    //将字符串转成C风格的
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();
    // 2.编译着色器程序
    unsigned int vertex, fragment;// 用于存储 OpenGL 创建的着色器对象 ID
    // ---------- 顶点着色器 ----------
    vertex = glCreateShader(GL_VERTEX_SHADER);// 创建一个顶点着色器对象
    // 将着色器源码绑定到该着色器对象
    // 参数说明：着色器ID，源码数量（通常为1），源码字符串数组指针，长度数组（NULL 表示以\0结尾）
    glShaderSource(vertex, 1, &vShaderCode, NULL);// 绑定源码
    glCompileShader(vertex);// 编译顶点着色器
    checkCompileErrors(vertex, "VERTEX");// 检查编译错误
    // ---------- 片段着色器 ----------
    fragment = glCreateShader(GL_FRAGMENT_SHADER);// 创建一个片段着色器对象
    glShaderSource(fragment, 1, &fShaderCode, NULL);// 绑定源码
    glCompileShader(fragment);// 编译片段着色器
    checkCompileErrors(fragment, "FRAGMENT");// 检查编译错误
    // ---------- 链接着色器程序 ----------
    ID = glCreateProgram();// 创建一个着色器程序对象
    glAttachShader(ID, vertex);// 将顶点着色器附加到程序
    glAttachShader(ID, fragment);// 将片段着色器附加到程序
    glLinkProgram(ID); // 链接程序（将两个着色器组合成一个可执行的 GPU 程序）
    checkCompileErrors(ID, "PROGRAM");// 检查链接是否成功
    // 释放已链接的着色器对象（因为它们已经集成到程序中，不再需要单独保留）
    // 注意：删除着色器对象不会影响已链接的程序
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

// 使用/激活shader
void Shader::use() {
    glUseProgram(ID);
}

// 设置 uniform 工具函数
void Shader::setBool(const std::string& name, bool value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}
void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}
// ------------------------------------------------------------------------
void Shader::setVec2(const std::string &name, const glm::vec2 &value) const { 
    glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); 
}
void Shader::setVec2(const std::string &name, float x, float y) const { 
    glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y); 
}
// ------------------------------------------------------------------------
void Shader::setVec3(const std::string &name, const glm::vec3 &value) const { 
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); 
}
void Shader::setVec3(const std::string &name, float x, float y, float z) const { 
    glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z); 
}
// ------------------------------------------------------------------------
void Shader::setVec4(const std::string &name, const glm::vec4 &value) const { 
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); 
}
void Shader::setVec4(const std::string &name, float x, float y, float z, float w) { 
    glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w); 
}
// ------------------------------------------------------------------------
void Shader::setMat2(const std::string &name, const glm::mat2 &mat) const {
    glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}
// ------------------------------------------------------------------------
void Shader::setMat3(const std::string &name, const glm::mat3 &mat) const {
    glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}
// ------------------------------------------------------------------------
void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

// 检查编译/链接错误
void Shader::checkCompileErrors(unsigned int shader, const std::string& type) {
    int success;
    char infoLog[1024];
    if (type != "PROGRAM") {//检查编译错误
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    } else { //检查链接错误
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    }
    
}