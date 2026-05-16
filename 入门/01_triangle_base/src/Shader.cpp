// ===========================================================================
// Shader.cpp —— Shader 类的实现
// ---------------------------------------------------------------------------
// 关键流程：
//   构造函数:
//     1) 从磁盘读取顶点 / 片段着色器源码字符串
//     2) 分别创建并编译顶点着色器对象与片段着色器对象
//     3) 创建着色器程序对象，把两个 shader 附加进来并 link
//     4) 编译 / 链接失败时打印日志；链接成功后删除单独的 shader 对象
//   setXxx:
//     通过 glGetUniformLocation + glUniformXxx 向当前程序上传 uniform 数据
// ===========================================================================

#include "Shader.h"

// ---------------------------------------------------------------------------
// 构造函数：加载、编译并链接着色器
// ---------------------------------------------------------------------------
Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    // ==================== 1. 读取 GLSL 源码文件 ====================
    std::string  vertexCode, fragmentCode;      // 读到的着色器源码字符串
    std::ifstream vShaderFile, fShaderFile;     // 文件输入流对象

    // 让 ifstream 在打开/读取失败时抛出异常（默认只是设置 failbit）
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        // 打开文件
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);

        // 用字符串流把整个文件缓冲区一次性读入内存
        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        // 从 stringstream 中取出 std::string
        vertexCode   = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e) {
        // 文件不存在、无权限、读取错误等都会进到这里
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
    }

    // OpenGL C API 要求传入 C 风格字符串 (const char*)
    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    // ==================== 2. 编译顶点 / 片段着色器 ====================
    unsigned int vertex, fragment; // OpenGL 创建的着色器对象 ID

    // ---------- 顶点着色器 ----------
    vertex = glCreateShader(GL_VERTEX_SHADER);      // 创建顶点着色器对象
    // glShaderSource(着色器ID, 源码数量, 源码字符串数组, 长度数组)
    //   - 源码数量通常为 1；长度数组传 NULL 表示源码以 '\0' 结尾
    glShaderSource(vertex, 1, &vShaderCode, NULL);  // 绑定源码
    glCompileShader(vertex);                        // 编译
    checkCompileErrors(vertex, "VERTEX");           // 打印编译错误（若有）

    // ---------- 片段着色器 ----------
    fragment = glCreateShader(GL_FRAGMENT_SHADER);  // 创建片段着色器对象
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    // ==================== 3. 链接着色器程序 ====================
    ID = glCreateProgram();                         // 创建程序对象
    glAttachShader(ID, vertex);                     // 挂载顶点着色器
    glAttachShader(ID, fragment);                   // 挂载片段着色器
    glLinkProgram(ID);                              // 链接：生成最终可运行在 GPU 上的程序
    checkCompileErrors(ID, "PROGRAM");              // 检查链接错误

    // 链接完成后，单独的着色器对象已经不再需要（其代码已嵌入程序对象），
    // 删除它们不会影响已链接的程序
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

// ---------------------------------------------------------------------------
// 激活当前着色器程序：在渲染调用前必须调用一次
// ---------------------------------------------------------------------------
void Shader::use() {
    glUseProgram(ID);
}

// ---------------------------------------------------------------------------
// uniform 设置工具函数：通过名字查找 uniform location，再上传数据
// 说明：
//   - glGetUniformLocation 若找不到该 uniform（或其被 GLSL 编译器优化掉）会返回 -1
//   - glUniform1i 同时用于设置 bool / int / sampler
//   - glUniformXfv 的 v 表示 "指针版本"，可一次上传多组数据（这里只传 1 组）
// ---------------------------------------------------------------------------
void Shader::setBool(const std::string& name, bool value) const {
    // GLSL 中没有专门的 bool uniform 设置函数，统一按 int 处理
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

// ---- vec2 ----
void Shader::setVec2(const std::string &name, const glm::vec2 &value) const {
    // &value[0] 拿到 glm::vec2 内部连续存储的浮点数组首地址
    glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}
void Shader::setVec2(const std::string &name, float x, float y) const {
    glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
}

// ---- vec3 ----
void Shader::setVec3(const std::string &name, const glm::vec3 &value) const {
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}
void Shader::setVec3(const std::string &name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}

// ---- vec4 ----
void Shader::setVec4(const std::string &name, const glm::vec4 &value) const {
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}
void Shader::setVec4(const std::string &name, float x, float y, float z, float w) {
    glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
}

// ---- 矩阵 ----
// glUniformMatrixXfv 参数：
//   location / 矩阵数量 / 是否转置 / 数据指针
// GLM 默认按列主序存储，与 OpenGL 一致，所以 transpose 填 GL_FALSE
void Shader::setMat2(const std::string &name, const glm::mat2 &mat) const {
    glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}
void Shader::setMat3(const std::string &name, const glm::mat3 &mat) const {
    glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}
void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

// ---------------------------------------------------------------------------
// 错误检查：根据 type 判断查询 "编译状态" 还是 "链接状态"，失败则输出日志
//   type == "PROGRAM"  : 查询着色器程序链接结果
//   type == 其他字符串: 查询着色器对象编译结果（如 "VERTEX" / "FRAGMENT"）
// ---------------------------------------------------------------------------
void Shader::checkCompileErrors(unsigned int shader, const std::string& type) {
    int  success;         // 状态标记：成功 = 非 0
    char infoLog[1024];   // OpenGL 返回的错误日志缓冲

    if (type != "PROGRAM") {
        // ---- 着色器编译错误 ----
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                      << infoLog << std::endl;
        }
    } else {
        // ---- 程序链接错误 ----
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                      << infoLog << std::endl;
        }
    }
}
