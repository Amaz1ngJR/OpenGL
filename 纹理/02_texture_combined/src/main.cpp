#include "Shader.h"
#include <GLFW/glfw3.h>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

GLenum Get_format(int nrChannels) {
    if (nrChannels == 1)
        return GL_RED;
    else if (nrChannels == 4)
        return GL_RGBA;
    else
        return GL_RGB;
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

    // 顶点数据
    /*
	位置坐标范围是 左下(-1, -1) - 右上(1, 1)
	纹理坐标范围是 左下(0, 0) - 右上(1, 1)
	*/
    float vertices[] = {
		// positions          // colors           // texture coords 正常范围是(0,0 - 1,1)
		 0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // 右上
		 0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // 右下
		-0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // 左下
		-0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // 左上
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
	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// texture coord attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

    // load and create a texture 
	// 创建纹理 texture
	unsigned int texture0, texture1;
	glGenTextures(1, &texture0);
	glGenTextures(1, &texture1);

	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
    glBindTexture(GL_TEXTURE_2D, texture0);
	unsigned char* data = stbi_load("../../../resources/textures/awesomeface.png", &width, &height, &nrChannels, 0);
	if (data)
	{
        GLenum format = Get_format(nrChannels);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		/*
			第一个参数指定了纹理目标(Target)。设置为GL_TEXTURE_2D意味着会生成与当前绑定的纹理对象在同一个目标上的纹理（任何绑定到GL_TEXTURE_1D和GL_TEXTURE_3D的纹理不会受到影响）。
			第二个参数为纹理指定多级渐远纹理的级别，如果你希望单独手动设置每个多级渐远纹理的级别的话。这里我们填0，也就是基本级别。
			第三个参数告诉OpenGL我们希望把纹理储存为何种格式。我们的图像只有RGB值，因此我们也把纹理储存为RGB值。
			第四个和第五个参数设置最终的纹理的宽度和高度。我们之前加载图像的时候储存了它们，所以我们使用对应的变量。
			下个参数应该总是被设为0（历史遗留的问题）。
			第七第八个参数定义了源图的格式和数据类型。我们使用RGB值加载这个图像jpg/ RGBA加载png，并把它们储存为char(byte)数组，我们将会传入对应值。
			最后一个参数是真正的图像数据。
		*/

		glGenerateMipmap(GL_TEXTURE_2D);

		// 设置纹理的环绕方式
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

		/*
		GL_REPEAT	对纹理的默认行为。重复纹理图像。
		GL_MIRRORED_REPEAT	和GL_REPEAT一样，但每次重复图片是镜像放置的。
		GL_CLAMP_TO_EDGE	纹理坐标会被约束在0到1之间，超出的部分会重复纹理坐标的边缘，产生一种边缘被拉伸的效果。
		GL_CLAMP_TO_BORDER	超出的坐标为用户指定的边缘颜色。
		*/

		// 设置多级渐远纹理
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);//注意设置这个缩小的时候
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		/*
		GL_NEAREST_MIPMAP_NEAREST	使用最邻近的多级渐远纹理来匹配像素大小，并使用邻近插值进行纹理采样
		GL_LINEAR_MIPMAP_NEAREST	使用最邻近的多级渐远纹理级别，并使用线性插值进行采样
		GL_NEAREST_MIPMAP_LINEAR	在两个最匹配像素大小的多级渐远纹理之间进行线性插值，使用邻近插值进行采样
		GL_LINEAR_MIPMAP_LINEAR	在两个邻近的多级渐远纹理之间使用线性插值，并使用线性插值进行采样
		*/

	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);

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

	//解绑VBO VAO EBO对象
	glBindBuffer(GL_ARRAY_BUFFER, 0);//可以先解绑VBO 因为glVertexAttribPointer函数会把VBO绑定到VAO上
	// glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //EBO 是 VAO 的一部分，当你 glBindVertexArray(0) 后，EBO 自动解绑
	glBindVertexArray(0);//VAO会记录EBO，所以最后解绑

    Shader ourShader("../shader/shader.vs", "../shader/shader.fs");

    // 主循环
    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.2f, 0.3f, 0.5f, 1.0f);// 设置清屏颜色并清除颜色缓冲
        glClear(GL_COLOR_BUFFER_BIT);//状态使用函数 清空屏幕的颜色缓冲

        //绘制三角形
		ourShader.use();
        glBindVertexArray(VAO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);//读出6个元素

        //OpenGL至少保证有16个纹理单元供你使用
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, texture1);
		//设置uniform
		ourShader.setInt("texture1", 0);
		ourShader.setInt("texture1", 1);

        glfwSwapBuffers(window);// 交换缓冲区
        glfwPollEvents(); // 处理轮询IO事件
    }

    //释放内存
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(ourShader.ID);

    glfwDestroyWindow(window);// 销毁窗口
    glfwTerminate();// glfw: 回收前面分配的GLFW相关资源
    return 0;
}
