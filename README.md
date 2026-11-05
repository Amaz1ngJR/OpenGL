# LearnOpenGL

本项目是跟随 [LearnOpenGL 中文教程](https://learnopengl-cn.github.io/) 编写的 C++17 / OpenGL 3.3 Core 学习程序。现在统一由一个引擎入口创建窗口、维护输入和渲染循环；教程场景作为引擎中的独立模块，通过 ImGui 面板切换。

## 目录结构

```text
.
├── engine/
│   ├── include/                 # 场景、着色器、相机接口
│   └── src/
│       ├── main.cpp             # 唯一窗口、ImGui 与主循环
│       ├── Scenes.cpp           # 教程场景与场景工厂
│       ├── ModelScene.cpp       # Assimp 静态模型、材质纹理与预览相机
│       ├── FileDialog.cpp       # Windows 原生模型选择器
│       ├── FileDialogMac.mm     # macOS 原生模型选择器
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
│   ├── assimp/                  # Assimp 5.4.3 源码（含内置 zlib）
│   └── stb_image.h              # 图片加载
├── cmake/                      # 两种构建共享的 Assimp 配置与 xmake 桥接工程
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
6. **Model viewer**：通过原生文件对话框选择本地模型，显示基础材质和贴图，并使用键鼠调整视角。

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

需要 C++17 编译器、OpenGL 3.3 Core Profile 和 **CMake ≥ 3.22**。可直接使用 CMake，或使用 xmake ≥ 2.9.9（xmake 会调用 CMake 构建 Assimp，因此两种方式都须安装 CMake 并加入 PATH）。GLFW、GLAD、GLM、ImGui 和 Assimp 源码均已包含在仓库，构建时不需要从网络下载依赖。

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

两个构建系统的生成文件分开放置：CMake 输出在 `build/cmake/`，xmake 输出在 `build/xmake/`。程序使用构建时写入的仓库根路径定位着色器和纹理，因此可以从任意工作目录启动。Windows 多配置生成器的运行命令见下方 Windows 小节。

### 场景冒烟测试

冒烟测试使用隐藏 GLFW 窗口，轮流创建、绘制、销毁所有场景三轮，并检查 framebuffer 是否有预期输出及是否出现 OpenGL 错误：

```bash
./build/cmake/OpenGLLearn --smoke-test
# 或先用 xmake 构建，再执行 xmake 输出目录中的程序：
xmake run OpenGLLearn --smoke-test
```

## 本地模型预览

仓库在 `resources/models/` 按“一模型一文件夹”保留可直接打开的 Assimp 学习模型：`cube/` 用于查看几何导入，`textured-quad/` 用于查看外部贴图加载。模型的材质与贴图放在各自模型目录中，请勿拆开移动。点击主面板的 **Load local model...** 可打开本地模型，也可以使用这些仓库示例。也可以先选择 Model viewer，再点 **Open local model...**。macOS 使用 NSOpenPanel，Windows 使用原生文件对话框；取消选择不改变当前场景，导入失败会显示错误并保留旧模型。

- 导入时处理节点变换、三角化和法线生成；模型自动居中并缩放至合适大小。
- **W/A/S/D** 移动，**按住鼠标右键拖动**改变朝向，**滚轮**改变视野，**Reset view** 恢复初始视角。请在 ImGui 面板外操作，避免与控件交互冲突。
- 支持基础材质颜色、外部及内嵌 diffuse/base-color 贴图，外部贴图相对模型目录解析。缺失或无法解码的贴图会给出警告，并回退到材质本色。
- 如果贴图上下颠倒，可勾选 **Flip texture V**。OBJ 的 `.mtl` 及贴图、glTF 的 `.bin` 等关联文件应随模型保留。
- 当前是静态模型预览，不播放骨骼动画，不实现完整 PBR、透明混合及法线/金属度贴图。图片解码受 stb_image 支持格式限制。
- 模型加载同步完成，大型模型可能暂时阻塞界面。中文文件路径使用 UTF-8；ImGui 默认字体仍不含完整中文字形，路径文字可能显示为方块，不影响实际路径解析。

现有冒烟入口也可指定示例或自己的模型，验证实际网格渲染（未增加独立测试目标）：

```bash
./build/cmake/OpenGLLearn --smoke-test --model "resources/models/cube/cube.obj"
xmake run OpenGLLearn --smoke-test --model "resources/models/textured-quad/quad.obj"
```

## Assimp 模型导入库

已集成 **Assimp 5.4.3**：CMake 和 xmake 使用同一份 `third_party/assimp` 源码及 `cmake/AssimpOptions.cmake` 配置。Assimp、内置 zlib 均静态链接，Windows 无需额外拷贝 Assimp/zlib DLL；MSVC 运行库保持 Debug `/MDd`、Release `/MD` 一致（运行机器仍需相应 Visual C++ 运行库）。

- 默认开启上游导入器集合，包含 OBJ、FBX、glTF/GLB 等；不启用 USD、Draco 压缩扩展和模型导出功能。
- xmake 的 Assimp 产物位于 `build/xmake-assimp/<平台>/<架构>/<模式>/`，不与主 CMake 构建混用。
- 模型预览场景使用 `engine/src/ModelScene.cpp` 与 `resources/shaders/model/`，通过 Assimp 导入静态模型并绘制基础颜色/漫反射贴图。
- `Assimp::Importer` 拥有 `ReadFile()` 返回的 `aiScene`；Importer 析构或再次导入后指针失效，渲染层须提前复制需要的顶点/索引/材质数据。
- 版本、上游提交和许可证说明见 `third_party/assimp/INTEGRATION.md`。

### Windows（Visual Studio 2019/2022 MSVC）

安装 Visual Studio 的“使用 C++ 的桌面开发”工作负载、Windows SDK 和 CMake ≥ 3.22，以下命令使用 x64 架构。Windows xmake 配置使用 MSVC，不支持混用 MinGW 编译出的库。

```powershell
# CMake 多配置生成器：运行路径中的配置要与 --config 保持一致
cmake -S . -B build/cmake-vs -G "Visual Studio 17 2022" -A x64
cmake --build build/cmake-vs --config Release --parallel 4
.\build\cmake-vs\Release\OpenGLLearn.exe

# xmake（Debug 时改为 -m debug）
xmake f -p windows -a x64 -m release --toolchain=msvc --vs=2022
xmake
xmake run OpenGLLearn
```

VS 2019 对应 CMake 生成器 `Visual Studio 16 2019` 和 xmake `--vs=2019`。首次编译 Assimp 的时间和磁盘开销较大，后续增量构建会复用产物。

## 添加教程场景

1. 在 `resources/shaders/` 下按类别保存该场景的 `.vs` 和 `.fs` 文件。
2. 在 `engine/src/Scenes.cpp` 中实现一个 `Scene` 派生类。场景构造函数创建所需 GPU 资源，`render()` 绘制场景，析构时释放资源。
3. 在 `sceneInfos` 中增加面板名称，并在 `createScene()` 的工厂分支中返回场景实例；保持列表顺序与工厂索引一致。
4. 若场景需要相机输入，继承 `CameraScene` 或实现 `usesCamera()` 与 `input()`。
5. 使用 CMake 和 xmake 构建，并运行 `--smoke-test` 验证。

`Mesh`、`Texture` 和 `Shader` 都遵循 RAII 管理 OpenGL 对象。销毁场景时必须仍有有效的 OpenGL 上下文，不能把 GPU 资源释放延迟到窗口销毁之后。

## 开发验证范围

当前在 macOS Apple Silicon 上验证 CMake、xmake 构建、全部场景的三轮 OpenGL 冒烟测试。Windows 已配置 MSVC 架构、CRT 与静态库链接，但尚未在 Windows 实机验证；不能将跨平台构建配置视为已经通过 Windows 测试。其他平台的 GLFW 系统依赖尚未在本项目验证。
