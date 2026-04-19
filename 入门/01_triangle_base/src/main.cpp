#include "Shader.h"
#include <GLFW/glfw3.h>

const unsigned int SCR_WIDTH = 800;//窗口的宽
const unsigned int SCR_HEIGHT = 600;//窗口的高
// 错误回调
void errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error: " << description << std::endl;
}
// 键盘回调
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// glfw:当改变窗口大小的时候执行的回调函数
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);//设置窗口的维度 处理过的OpenGL坐标范围只为-1到1
}

int main() {
    glfwSetErrorCallback(errorCallback);// 设置错误回调
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    // 设置 OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Amaz1ng", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);// 将窗口的上下文设置为当前线程的上下文
    glfwSetKeyCallback(window, keyCallback);// 设置键盘回调
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); // 设置窗口大小回调
    // 启用垂直同步 (VSync)
    glfwSwapInterval(1);//1开启 0关闭

    // 初始化 GLAD 加载所有OpenGL函数指针
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);//第二个参数默认是fill填充的
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // 顶点数据：三个点构成三角形
    float vertices[] = {//(x, y, z, R, G, B)
		 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, // left  
		-0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, // right 
		 0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f  // top   
	};

    //创建VBO VAO 对象 并赋予ID
	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	//绑定VBO VAO对象
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);//VBO的缓冲类型是GL_ARRAY_BUFFER
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	//告知shader如何解析缓冲里的属性(index,3个,float,是否标准化,步长,偏移量)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);//开启VAO管理的第一个属性值
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);//开启VAO管理的第一个属性值
	//解绑VBO VAO对象
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

    Shader ourShader("../shader/shader.vs", "../shader/shader.fs");

    // 主循环
    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2f, 0.3f, 0.5f, 1.0f);// 设置清屏颜色并清除颜色缓冲
        glClear(GL_COLOR_BUFFER_BIT);//状态使用函数 清空屏幕的颜色缓冲

        //绘制三角形
		ourShader.use();
        glBindVertexArray(VAO);//绑定VAO
		glDrawArrays(GL_TRIANGLES, 0, 3);//绘制三角形

        glfwSwapBuffers(window);// 交换缓冲区
        glfwPollEvents(); // 处理轮询IO事件
    }

    //释放内存
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
    glDeleteProgram(ourShader.ID);

    glfwDestroyWindow(window);// 销毁窗口
    glfwTerminate();// glfw: 回收前面分配的GLFW相关资源
    return 0;
}
