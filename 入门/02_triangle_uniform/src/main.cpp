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

    // // 顶点数据
    float vertices[] = {
		 0.5f, 0.5f, 0.0f,  // 右上
		 0.5f, -0.5f, 0.0f, // 右下  
		 -0.5f, -0.5f, 0.0f, // 左下 
		 -0.5f,  0.5f, 0.0f  // 左上   
	};
	unsigned int indices[] = {
		0, 1, 3, // 第一个三角形
		1, 2, 3  // 第二个三角形
	};

    //创建VBO VAO EBO对象 并赋予ID
	unsigned int VBO, VAO, EBO;
	glGenVertexArrays(1, &VAO);//生成1个VAO对象
	glGenBuffers(1, &VBO);//生成1个VBO对象
	glGenBuffers(1, &EBO);//生成1个EBO对象
	//绑定VBO VAO对象
	glBindVertexArray(VAO);//绑定VAO对象
    // 把顶点数组复制到缓冲中供OpenGL使用
	glBindBuffer(GL_ARRAY_BUFFER, VBO);//VBO的缓冲类型是GL_ARRAY_BUFFER
	//为当前绑定到target的缓冲区对象创建一个新的数据存储
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	/* glBufferData 第四个参数
	GL_STATIC_DRAW ：数据不会或几乎不会改变。
	GL_DYNAMIC_DRAW：数据会被改变很多。
	GL_STREAM_DRAW ：数据每次绘制时都会改变。
	*/
	//绑定EBO对象 索引
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);//EBO的缓冲类型是GL_ELEMENT_ARRAY_BUFFER
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	//告知shader如何解析缓冲里的属性(index,3个,float,是否标准化,步长,偏移量)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);//位置0 3个float类型数据
    /*第一个参数指定我们要配置的顶点属性位置(位置值的索引)
    第二个参数指定顶点属性的大小(每个顶点属性的组件数量)
    第三个参数指定数据的类型
    第四个参数定义我们是否希望数据被标准化(映射到0到1之间)
    第五个参数叫做步长(连续顶点属性之间的间隔)
    第六个参数类型是void* 表示位置数据在缓冲中起始位置的偏移量 
    */
	glEnableVertexAttribArray(0);//开启VAO管理的第一个属性值

	//解绑VBO VAO EBO对象
	glBindBuffer(GL_ARRAY_BUFFER, 0);//可以先解绑VBO 因为glVertexAttribPointer函数会把VBO绑定到VAO上
	// glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);//EBO 是 VAO 的一部分，当你 glBindVertexArray(0) 后，EBO 自动解绑
	glBindVertexArray(0);//VAO会记录EBO，所以最后解绑

    Shader ourShader("../shader/shader.vs", "../shader/shader.fs");

    // 主循环
    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2f, 0.3f, 0.5f, 1.0f);// 设置清屏颜色并清除颜色缓冲
        glClear(GL_COLOR_BUFFER_BIT);//状态使用函数 清空屏幕的颜色缓冲

        //绘制三角形
		ourShader.use();
         //更新uniform颜色
        float timeValue = glfwGetTime();
        float greenValue = (sin(timeValue) / 2.0f) + 0.5f;
        int vertexColorLocation = glGetUniformLocation(ourShader.ID, "ourColor");
        glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f); //更新uniform颜色
        glBindVertexArray(VAO);//绑定VAO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);//绑定EBO
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);//使用EBO索引绘制三角形

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
