// ===========================================================================
// main.cpp —— OpenGL 入门示例：绘制一个带渐变颜色的三角形
// ---------------------------------------------------------------------------
// 主要流程：
//   1) 初始化 GLFW 并配置 OpenGL 3.3 Core Profile 上下文
//   2) 创建窗口并使之成为当前线程的渲染上下文
//   3) 使用 GLAD 加载当前平台的 OpenGL 函数指针
//   4) 准备顶点数据（位置 + 颜色），构建 VBO / VAO
//   5) 通过 Shader 类加载并编译顶点/片段着色器
//   6) 主循环：清屏 → 绑定 VAO → 调用 glDrawArrays 绘制三角形 → 交换缓冲
//   7) 退出前释放 GPU 资源与 GLFW 资源
// ===========================================================================

#include "Shader.h"          // 自定义的 Shader 类：负责从文件加载、编译、链接 GLSL 源码
#include <GLFW/glfw3.h>      // GLFW：跨平台窗口与输入事件库

// ----------------------- 全局常量：窗口大小 --------------------------------
const unsigned int SCR_WIDTH  = 800; // 窗口的宽（像素）
const unsigned int SCR_HEIGHT = 600; // 窗口的高（像素）

// ----------------------- GLFW 错误回调 -------------------------------------
// 当 GLFW 内部发生错误时会自动调用这里设置的回调函数，便于定位问题
void errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error: " << description << std::endl;
}

// ----------------------- 键盘输入回调 --------------------------------------
// 按下 ESC 键时向窗口设置 "应当关闭" 标志，让主循环自然退出
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// ----------------------- 窗口尺寸变化回调 ----------------------------------
// 用户拖动窗口改变大小时调用；需要同步更新 OpenGL 视口
// 注意：OpenGL 的归一化设备坐标 (NDC) 永远是 [-1, 1]，glViewport 只是把 NDC
//       映射到实际的窗口像素范围，因此任何时候窗口尺寸变动都要重新设置。
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height); // 设置视口：左下角为 (0,0)，宽高为窗口大小
}

int main() {
    // -------- 1. 初始化 GLFW --------
    glfwSetErrorCallback(errorCallback); // 注册错误回调（应在 glfwInit 之前设置）
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // -------- 2. 请求 OpenGL 3.3 Core Profile 上下文 --------
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);                  // 主版本号 = 3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);                  // 次版本号 = 3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // Core Profile：不含已弃用的固定管线 API

#ifdef __APPLE__
    // macOS 必须显式开启 "前向兼容" 才能成功创建 3.3+ Core 上下文
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // -------- 3. 创建窗口 --------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Amaz1ng", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate(); // 创建失败要释放前面 glfwInit 申请的资源
        return -1;
    }

    glfwMakeContextCurrent(window);                              // 将该窗口的 OpenGL 上下文设为当前线程的上下文
    glfwSetKeyCallback(window, keyCallback);                     // 注册键盘回调
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); // 注册窗口大小变化回调

    // 启用垂直同步 (VSync)：交换缓冲区时等待显示器刷新，避免画面撕裂
    // 参数 1 = 开启（每次刷新交换一次），0 = 关闭（尽可能快）
    glfwSwapInterval(1);

    // -------- 4. 使用 GLAD 加载 OpenGL 函数指针 --------
    // 必须在有当前上下文之后、调用任何 gl* 函数之前执行
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 调试技巧：若取消下面这行注释，则以线框模式绘制（方便观察三角形的三条边）
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // 第二个参数默认是 GL_FILL（填充）
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // -------- 5. 顶点数据：三个顶点构成一个三角形 --------
    // 每个顶点 6 个 float：位置 (x, y, z) + 颜色 (R, G, B)
    // 由于没有用透视/视图矩阵变换，这里的坐标就是 NDC，范围 [-1, 1]
    float vertices[] = {// (x, y, z, R, G, B)
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, // 右下顶点：红色
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, // 左下顶点：绿色
         0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f  // 上方顶点：蓝色
    };

    // -------- 6. 创建并配置 VBO / VAO --------
    // VBO (Vertex Buffer Object)：在 GPU 上开辟一块显存存放顶点原始数据
    // VAO (Vertex Array Object)：记录 "如何解释 VBO 中数据" 的状态集合；
    //                            绑定 VAO 时会自动把关联的 VBO + 属性配置一起恢复
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO); // 生成 1 个 VAO，把 ID 写入 VAO 变量
    glGenBuffers(1, &VBO);      // 生成 1 个 VBO

    // 先绑定 VAO，之后所有关于顶点属性/缓冲的配置都会记录进这个 VAO
    glBindVertexArray(VAO);

    // 把 VBO 绑定到 GL_ARRAY_BUFFER 目标（顶点缓冲专用目标）
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // 把 CPU 侧的 vertices 上传到 GPU
    // 参数：目标 / 数据字节数 / 数据指针 / 使用方式（STATIC 表示数据不常变动）
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // -------- 告知着色器如何解析顶点缓冲中的属性 --------
    // glVertexAttribPointer 参数：
    //   1) location  —— 对应 shader 中 layout(location = ?)
    //   2) size      —— 每个属性由多少个分量组成（vec3 = 3）
    //   3) type      —— 数据类型
    //   4) normalized—— 是否需要将整数归一化到 [0,1] / [-1,1]
    //   5) stride    —— 相邻两个顶点之间的字节跨度（6 * float）
    //   6) offset    —— 该属性在一个顶点内的起始字节偏移
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); // 启用 location = 0 的属性（位置）
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1); // 启用 location = 1 的属性（颜色，从第 4 个 float 开始）

    // 解绑，避免后续操作意外污染当前 VAO / VBO 的状态
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // -------- 7. 加载并编译着色器程序 --------
    // 路径相对于可执行文件运行时的工作目录；因此项目里把 shader 文件夹放在 build/.. 上层
    Shader ourShader("../shader/shader.vs", "../shader/shader.fs");

    // -------- 8. 渲染主循环 --------
    while (!glfwWindowShouldClose(window)) {
        // 每帧先用背景色清屏，否则会残留上一帧的内容
        glClearColor(0.2f, 0.3f, 0.5f, 1.0f); // 状态设置函数：指定清屏颜色 (RGBA)
        glClear(GL_COLOR_BUFFER_BIT);         // 状态使用函数：用上面的颜色清空颜色缓冲

        // 绘制三角形：激活着色器 → 绑定 VAO → 发起 DrawCall
        ourShader.use();
        glBindVertexArray(VAO);
        // glDrawArrays(图元类型, 起始索引, 顶点数量)
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window); // 交换前后缓冲（双缓冲防止撕裂）
        glfwPollEvents();        // 轮询并派发所有待处理的事件（输入/窗口等）
    }

    // -------- 9. 释放资源 --------
    glDeleteVertexArrays(1, &VAO);     // 删除 VAO
    glDeleteBuffers(1, &VBO);          // 删除 VBO
    glDeleteProgram(ourShader.ID);     // 删除着色器程序对象

    glfwDestroyWindow(window); // 销毁窗口
    glfwTerminate();           // 释放 GLFW 内部全局资源
    return 0;
}
