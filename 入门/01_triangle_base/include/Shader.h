// ===========================================================================
// Shader.h —— 轻量级 GLSL 着色器封装类
// ---------------------------------------------------------------------------
// 作用：把 "读取 GLSL 源码 → 编译顶点/片段着色器 → 链接着色器程序 → 设置
//       uniform" 这一整套 OpenGL 固定流程封装成一个易用的类，避免在主程序
//       中重复编写冗长的样板代码。
//
// 使用示例：
//     Shader ourShader("shader.vs", "shader.fs"); // 构造时会读取并编译链接
//     ourShader.use();                             // 激活该着色器程序
//     ourShader.setFloat("time", 0.5f);            // 设置 uniform
// ===========================================================================

#pragma once                // 头文件保护：避免同一个头文件被重复包含

#include <glad/glad.h>      // GLAD：加载 OpenGL 函数指针（必须在 GLFW 之前 include）
#include <glm/glm.hpp>      // GLM：OpenGL 数学库，提供 vec2 / vec3 / vec4 / mat2 / mat3 / mat4 等类型
#include <string>           // std::string：存储 uniform 名称
#include <fstream>          // std::ifstream：读取 shader 源码文件
#include <sstream>          // std::stringstream：将文件内容拼接为字符串
#include <iostream>         // std::cout / std::cerr：打印错误信息

class Shader
{
public:
    // 构造函数：传入顶点着色器与片段着色器的文件路径，完成读取、编译、链接
    Shader(const char* vertexPath, const char* fragmentPath);

    unsigned int ID;    // OpenGL 生成的着色器程序对象 ID（glCreateProgram 返回值）

    // 激活（使用）当前着色器程序；渲染前必须先调用
    void use();

    // ---------------- uniform 设置工具函数 ----------------
    // 所有 setXxx 都会先用 glGetUniformLocation 获取 uniform 变量在程序中的位置，
    // 再通过 glUniformXxx 把 CPU 侧的数据上传到 GPU 对应的 uniform。
    // name 参数需与 GLSL 源码中 uniform 的变量名完全一致。

    void setBool (const std::string &name, bool  value) const;  // 上传 bool  (底层用 int 存储)
    void setInt  (const std::string &name, int   value) const;  // 上传 int
    void setFloat(const std::string &name, float value) const;  // 上传 float

    // ---- vec2 / vec3 / vec4：分别提供 "传 glm 向量" 与 "逐分量传" 两个重载 ----
    void setVec2(const std::string &name, const glm::vec2 &value) const;
    void setVec2(const std::string &name, float x, float y) const;

    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;

    void setVec4(const std::string &name, const glm::vec4 &value) const;
    void setVec4(const std::string &name, float x, float y, float z, float w); // 注意：此重载未加 const

    // ---- 矩阵类型：2x2 / 3x3 / 4x4 ----
    void setMat2(const std::string &name, const glm::mat2 &mat) const;
    void setMat3(const std::string &name, const glm::mat3 &mat) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;

private:
    // 检查着色器编译 / 程序链接是否成功；失败时打印 OpenGL 返回的错误日志
    //   shader: 着色器对象或着色器程序对象的 ID
    //   type  : "VERTEX" / "FRAGMENT" 表示检查着色器编译错误；
    //           "PROGRAM" 表示检查程序链接错误
    void checkCompileErrors(unsigned int shader, const std::string& type);
};
