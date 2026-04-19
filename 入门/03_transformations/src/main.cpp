#include "Shader.h"
#include <GLFW/glfw3.h>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Camera.h"
#include<glm/gtc/type_ptr.hpp>

// 窗口宽度和高度
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// 相机初始化
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// 时间管理变量
float deltaTime = 0.0f;	// 当前帧与上一帧的时间差
float lastFrame = 0.0f;	// 上一帧的时间

// 错误回调函数，用于处理GLFW错误
void errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error: " << description << std::endl;
}

// 键盘输入回调函数
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true); // 按下ESC键关闭窗口
        
    // 根据按键控制相机移动
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// 鼠标移动回调函数
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) { // 如果是第一次获取鼠标位置
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // 注意：在屏幕坐标系中，y轴向下为正方向
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset); // 更新相机角度
}

// 鼠标滚轮回调函数
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset)); // 根据滚轮滚动更新相机焦距
}

// 改变窗口大小时的回调函数
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height); // 设置OpenGL视口大小
}

// 根据通道数确定纹理格式
GLenum Get_format(int nrChannels) {
    if (nrChannels == 1)
        return GL_RED;
    else if (nrChannels == 4)
        return GL_RGBA;
    else
        return GL_RGB;
}

int main() {
	// 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);//大版本设为3  3._
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);//小版本设为3  _.3 -》3.3
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // 创建窗口对象
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Amaz1ng", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);// 将窗口的上下文设置为当前线程的上下文
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // 可选：隐藏鼠标光标
    glfwSetKeyCallback(window, keyCallback);// 设置键盘回调
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); // 设置窗口大小回调
    glfwSetCursorPosCallback(window, mouse_callback);      // 鼠标移动
    glfwSetScrollCallback(window, scroll_callback);        // 鼠标滚轮
    glfwSwapInterval(1); // VSync 启用垂直同步

	// glad: 加载所有OpenGL函数指针
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);//第二个参数默认是fill填充的

    Shader ourShader("../shader/shader.vs", "../shader/shader.fs");
    
    /*
	位置坐标范围是 左下(-1, -1) - 右上(1, 1)
	纹理坐标范围是 左下(0, 0) - 右上(1, 1)
	*/
	float vertices[] = {// 定义顶点数据（包括位置和纹理坐标
	-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
	 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

	-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
	};

    // // 定义多个立方体的世界坐标
    glm::vec3 cubePositions[] = {
        glm::vec3( 0.0f,  0.0f,  0.0f),
        glm::vec3( 2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f, -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3 (2.4f, -0.4f, -3.5f),
        glm::vec3(-1.7f,  3.0f, -7.5f),
        glm::vec3( 1.3f, -2.0f, -2.5f),
        glm::vec3( 1.5f,  2.0f, -2.5f),
        glm::vec3( 1.5f,  0.2f, -1.5f),
        glm::vec3(-1.3f,  1.0f, -1.5f)
    };

	//创建VBO VAO EBO对象 并赋予ID
	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	//绑定VBO VAO对象
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);//VBO的缓冲类型是GL_ARRAY_BUFFER
	//为当前绑定到target的缓冲区对象创建一个新的数据存储
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	//告知shader如何解析缓冲里的属性(index,3个,float,是否标准化,步长,偏移量)
	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// texture coord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// load and create a texture 
	// 创建纹理 texture
	unsigned int texture0, texture1;
	glGenTextures(1, &texture0);

	glBindTexture(GL_TEXTURE_2D, texture0);
	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
	unsigned char* data = stbi_load("../../../resources/textures/awesomeface.png", &width, &height, &nrChannels, 0);
	if (data)
	{
        GLenum format = Get_format(nrChannels);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

		// 设置纹理的环绕方式
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

		// 设置多级渐远纹理
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);//注意设置这个缩小的时候
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);

	glGenTextures(1, &texture1);
	glBindTexture(GL_TEXTURE_2D, texture1);
	data = stbi_load("../../../resources/textures/container.jpg", &width, &height, &nrChannels, 0);
	if (data)
	{
        GLenum format = Get_format(nrChannels);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);

		glGenerateMipmap(GL_TEXTURE_2D);

		// 设置纹理的环绕方式
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

		// 设置多级渐远纹理
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);//注意设置这个缩小的时候
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);


	// 渲染循环
	while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

        // 计算模型、视图、投影矩阵
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        // 设置着色器中的uniform变量
        ourShader.use();
		unsigned int transformLoc = glGetUniformLocation(ourShader.ID, "model");
		glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(model));//第三个参数设置矩阵行和列是否交换
		transformLoc = glGetUniformLocation(ourShader.ID, "view");
		glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(view));
		transformLoc = glGetUniformLocation(ourShader.ID, "projection");
		glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // 清除颜色缓冲区和深度缓冲区
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);//状态设置函数
		glEnable(GL_DEPTH_TEST); //启用深度测试
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);//状态使用函数 清空屏幕的颜色缓冲

        glBindVertexArray(VAO);// 绑定VAO
        // 绘制第一个立方体
		glDrawArrays(GL_TRIANGLES, 0, 36);

        // 绘制剩余的立方体
        for (unsigned int i = 1; i < 10; i++)
        {
            // calculate the model matrix for each object and pass it to shader before drawing
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            float angle = 20.0f * i;
            // model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            model = glm::rotate(model, (float)glfwGetTime() * glm::radians(angle), glm::vec3(0.5f, 1.0f, 0.0f));
            ourShader.setMat4("model", model);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

		//OpenGL至少保证有16个纹理单元供你使用
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, texture1);
		//设置uniform
		ourShader.setInt("texture0", 0);
		ourShader.setInt("texture1", 1);

		// glfw: 交换缓冲区和轮询IO事件(按键按下/释放、鼠标移动等)
		//前缓冲区：屏幕上显示的图像 后缓冲区：正在渲染的图像
		glfwSwapBuffers(window);//交换缓冲区
		glfwPollEvents();//轮询IO事件
	}
	//释放内存
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteProgram(ourShader.ID);

	// glfw: 回收前面分配的GLFW相关资源
	glfwTerminate();
	return 0;
}
