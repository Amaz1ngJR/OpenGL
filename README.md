# LearnOpenGL

本项目是跟随 [LearnOpenGL 中文教程](https://learnopengl-cn.github.io/) 编写的 C++17 / OpenGL 3.3 Core 学习程序。现在统一由一个引擎入口创建窗口、维护输入和渲染循环；教程场景作为引擎中的独立模块，通过 ImGui 面板切换。

## 目录结构

```text
.
├── engine/
│   ├── include/                 # 场景、着色器、相机接口
│   └── src/
│       ├── main.cpp             # 唯一窗口、ImGui 与主循环
│       ├── Scenes.cpp           # 场景工厂与五个教程场景
│       ├── Shader.cpp           # GLSL 编译/链接与 uniform 工具
│       └── Camera.cpp           # 共享 3D 摄像机
├── resources/
│   ├── shaders/                 # 按教程分类整理的 GLSL 文件
│   └── textures/                # 场景共用图片
├── third_party/
│   ├── glfw-3.3.8/              # 窗口和输入
│   ├── glad/                    # OpenGL 函数加载
│   ├── glm/                     # 向量、矩阵运算
│   ├── imgui/                   # ImGui 核心及 GLFW/OpenGL3 后端
│   └── stb_image.h              # 图片加载
├── CMakeLists.txt               # CMake 构建配置
└── xmake.lua                    # xmake 构建配置
```

`resources/shaders/` 延续教程分类，便于对照学习；场景的 C++ 渲染实现统一放在 `engine/`。窗口、GLFW 回调、ImGui 初始化和帧循环只有一份，各场景不再创建窗口或包含自己的 `main()`。

## 已实现的场景

启动时只显示一个窗口和 ImGui 控制面板，不绘制场景。面板中可单选：

1. **Triangle**：顶点颜色渐变三角形。
2. **Uniform rectangle**：通过 uniform 随时间变化颜色的矩形。
3. **Transformations**：纹理立方体阵列、模型变换和相机。
4. **Texture**：在同一个场景中切换单纹理/双纹理混合，并实时调整混合比例、UV 缩放、环绕方式和过滤方式。
5. **Phong lighting + materials**：在同一个场景中观察 Phong 环境光、漫反射、镜面反射，并实时调节光源轨迹、光源颜色和中心物体材质。

场景按需创建。切换时先创建新场景，创建成功后再销毁旧场景；场景独占的 shader、纹理和网格对象会随场景析构释放。3D 场景支持 **WASD** 移动、按住鼠标右键拖动视角、滚轮缩放；**ESC** 退出。ImGui 标签使用英文，避免默认字体缺少中文字形。

## 纹理参数面板

`Texture` 合并了原先的基础纹理和混合纹理演示，使用 `resources/shaders/textures/shader.vs/.fs`：

- `Combine textures`：启用双纹理混合；关闭后单独显示 awesomeface。
- `Texture mix`：awesomeface 在混合结果中的占比，范围 `0–1`，默认 `0.8`。
- `Vertex color tint`：是否叠加顶点颜色。
- `UV scale`：围绕纹理中心缩放坐标；设置大于 `1` 可直观看到超出 `[0,1]` 的采样效果。
- 展开每张纹理的设置后，可分别调整 `Wrap S`、`Wrap T`，支持 `GL_REPEAT`、`GL_MIRRORED_REPEAT`、`GL_CLAMP_TO_EDGE`、`GL_CLAMP_TO_BORDER`。使用边框钳制时可编辑 `Border color`。
- `Min filter` 支持 `GL_NEAREST`、`GL_LINEAR` 和四种 mipmap 过滤；`Mag filter` 仅支持 OpenGL 允许的 `GL_NEAREST`、`GL_LINEAR`。所有纹理加载后都会生成完整的 mipmap。

采样参数按纹理分别保存，在当前场景中实时生效；切换场景后重新使用默认值。若过滤方式变化不明显，需要调整 UV 缩放或观察距离，让纹理确实处于缩小或放大采样状态。

## 构建与运行

需要 C++17 编译器、OpenGL 3.3 Core Profile，以及 CMake ≥ 3.16 或 xmake。GLFW、GLAD、GLM 和 ImGui 源码均已包含在仓库，不需要下载 ImGui。

### CMake

在仓库根目录运行：

```bash
cmake -S . -B build/cmake
cmake --build build/cmake
./build/cmake/OpenGLLearn
```

### xmake

同样在仓库根目录运行：

```bash
xmake
xmake run OpenGLLearn
```

两个构建系统的生成文件分开放置：CMake 输出在 `build/cmake/`，xmake 输出在 `build/xmake/`。程序使用构建时写入的仓库根路径定位着色器和纹理，因此可以从任意工作目录启动。Windows 可使用 `build\\cmake\\OpenGLLearn.exe` 或 `xmake run OpenGLLearn`。

### 场景冒烟测试

冒烟测试使用隐藏 GLFW 窗口，轮流创建、绘制、销毁所有场景三轮，并检查 framebuffer 是否有预期输出及是否出现 OpenGL 错误：

```bash
./build/cmake/OpenGLLearn --smoke-test
# 或先用 xmake 构建，再执行 xmake 输出目录中的程序：
xmake run OpenGLLearn --smoke-test
```

## 添加教程场景

1. 在 `resources/shaders/` 下按类别保存该场景的 `.vs` 和 `.fs` 文件。
2. 在 `engine/src/Scenes.cpp` 中实现一个 `Scene` 派生类。场景构造函数创建所需 GPU 资源，`render()` 绘制场景，析构时释放资源。
3. 在 `sceneInfos` 中增加面板名称，并在 `createScene()` 的工厂分支中返回场景实例；保持列表顺序与工厂索引一致。
4. 若场景需要相机输入，继承 `CameraScene` 或实现 `usesCamera()` 与 `input()`。
5. 使用 CMake 和 xmake 构建，并运行 `--smoke-test` 验证。

`Mesh`、`Texture` 和 `Shader` 都遵循 RAII 管理 OpenGL 对象。销毁场景时必须仍有有效的 OpenGL 上下文，不能把 GPU 资源释放延迟到窗口销毁之后。

## 开发验证范围

当前统一程序已在 macOS Apple Silicon 上通过 CMake、xmake 构建及全部场景的三轮 OpenGL 冒烟测试。根目录 xmake 配置目前支持 macOS 和 Windows；其他平台的 GLFW 系统依赖尚未在本项目验证。
