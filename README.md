# LearnOpenGL

本项目是跟随 [LearnOpenGL 中文教程](https://learnopengl-cn.github.io/) 编写的 C++17 / OpenGL 3.3 Core 学习程序。现在统一由一个引擎入口创建窗口、维护输入和渲染循环；教程场景作为引擎中的独立模块，通过 ImGui 面板切换。

## 目录结构

```text
.
├── engine/
│   ├── include/                 # 场景、着色器、相机接口
│   └── src/
│       ├── main.cpp             # 唯一窗口、ImGui 与主循环
│       ├── SmokeTest.cpp        # 独立的场景冒烟测试与光源暂停/恢复验证
│       ├── Scenes.cpp           # 场景列表与工厂分发
│       ├── scenes/              # 每个教程场景独立文件及共用辅助代码
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

`resources/shaders/` 延续教程分类，便于对照学习；教程场景的 C++ 渲染实现按场景拆分到 `engine/src/scenes/`；模型预览保留在独立的 `engine/src/ModelScene.cpp`。`Scenes.cpp` 只维护场景列表及工厂分发。窗口、GLFW 回调、ImGui 初始化和帧循环只有一份，各场景不再创建窗口或包含自己的 `main()`。

## 已实现的场景

启动时只显示一个窗口和 ImGui 控制面板，不绘制场景。面板中可单选：

1. **Model viewer**：通过原生文件对话框选择本地模型，显示基础材质和贴图，并使用键鼠调整视角。
2. **Triangle**：顶点颜色渐变三角形。
3. **Uniform rectangle**：通过 uniform 随时间变化颜色的矩形。
4. **Texture**：在同一个场景中切换单纹理/双纹理混合，并实时调整混合比例、UV 缩放、环绕方式和过滤方式。
5. **Transform+Face culling**：纹理立方体阵列、模型变换、相机和可调面剔除；纹理调参集中在 Texture 场景。
6. **Phong lighting + materials**：在同一个场景中观察 Phong 环境光、漫反射、镜面反射，并实时调节光源轨迹、光源颜色和中心物体材质。
7. **Depth testing**：大理石箱子、金属地面和彩色测试平面组成的统一 3D 场景。
8. **Stencil testing**：分步查看模板遮罩与物体描边，比较模板函数、参考值和位掩码。
9. **Blending**：草丛透明片元丢弃、半透明窗户及可切换的距离排序。
10. **Framebuffers**：离屏渲染与原图、反色、灰度、锐化、模糊、边缘检测、复古色调后期处理。
11. **Cubemaps**：天空盒、立方体和球体，可分别切换反射/折射并调节折射率。
12. **Asteroid belt**：行星与环形岩石阵列，对比普通绘制和实例化绘制，可调整数量、分布与公转。

场景按需创建。切换时先创建新场景，创建成功后再销毁旧场景；场景独占的 shader、纹理和网格对象会随场景析构释放。3D 场景支持 **WASD** 移动、按住鼠标右键拖动视角、滚轮缩放；**ESC** 退出。ImGui 标签使用英文，避免默认字体缺少中文字形。

## Asteroid belt 小行星带与实例化

使用 `resources/models/planet/` 和 `resources/models/rock/` 的现有单材质模型及贴图。默认生成 2000 个小行星，固定随机种子，围绕中心行星分布；可将数量调节为 0..10000，修改半径和带宽，或开启公转动画。WASD 移动速度按场景尺寸提高，右键拖动与滚轮操作不变。

`Use instanced drawing` 开启时，小行星使用 `glDrawElementsInstanced` 一次提交；关闭时逐个更新 model uniform 并绘制，布局保持一致。每个实例的 `mat4` 占用属性位置 3..6，四列均设置 `glVertexAttribDivisor(..., 1)`，使矩阵按实例而非按顶点推进。矩阵只在数量/分布改变时上传，公转使用共享 uniform。面板显示 draw call 数量（包括行星），不将其误当作 GPU 耗时；实例化减少 CPU 提交开销，并不减少三角形或片元数量。

## Transform+Face culling 面剔除

`Enable face culling` 默认关闭；开启后通过 `Cull face` 选择：

- `GL_BACK`：只剔除背向面。
- `GL_FRONT`：只剔除正向面，可以观察闭合物体的内部表面。
- `GL_FRONT_AND_BACK`：同时剔除正反向三角形，立方体会完全不可见，这是预期效果。

正反面由投影到屏幕后的顶点绕序决定，与顶点法线无关；本场景使用 `glFrontFace(GL_CCW)`，即逆时针为正面。绘制结束会恢复先前的面剔除状态，不影响 ImGui 或其他场景。

## Depth testing 深度测试

深度测试只有一个 3D 场景：大理石箱子、金属地面，以及立在箱子之间的彩色平面。所有物体共用相机、透视投影和深度缓冲，WASD、右键拖动和滚轮均会影响它们。

默认开启 `Gray card depth reference`：先用 LESS 和独立深度写入绘制三张灰色参考平面，再使用面板中选择的深度参数绘制箱子、地面和彩色平面。橙色在参考平面前方，绿色重复参考平面的完全相同几何及变换，蓝色位于参考平面后方。这里的深度由真实投影产生，不再固定为屏幕深度值。

保持默认视角及其他参数，依次尝试：

- `LESS`：橙色通过，等深度的绿色与更远的蓝色被参考平面挡住。
- `LEQUAL`：绿色也能通过；`EQUAL` 可以单独观察绿色的等深度重绘。
- `GREATER` / `GEQUAL`：蓝色可在灰色参考区域内通过；其他未写入深度的位置仍为清除值 1，通常无法通过。
- `NOTEQUAL`：绿色因深度相同被拒绝；橙色和蓝色可通过，重叠处受前面绘制的几何影响。
- `ALWAYS`：后绘制覆盖前绘制；`NEVER`：正常场景绘制全部失败，只剩独立绘制的灰色参考平面。
- 关闭参考平面后，用 LESS 切换 `Write depth buffer`、`Draw floor first` 和 `Reverse colored card order`，比较深度写入及绘制顺序造成的遮挡变化。
- 关闭 `Enable depth test`，正常场景按绘制顺序覆盖，同时停止深度写入；独立参考平面仍按其固定规则绘制。

参考平面可随时关闭，此时全部场景几何只使用面板选择的深度规则。EQUAL 在没有预先写入相同几何深度时可能全空；GREATER 配合默认清除值 1 也可能全空，并非渲染故障。相机移动或裁剪面变化会改变实际遮挡，以上结果以默认视角为准。

`Display` 可切换纹理/彩色、线性深度 / 远平面，箱子、地面和测试平面统一生效。`Near plane` / `Far plane` 调整透视裁剪范围；线性深度显示的是沿观察轴的距离除以远平面，不会改写深度缓冲。深度清除值保持 1，绘制后恢复深度状态。`Reset depth settings` 恢复参数，`Reset camera` 恢复相机。

## Stencil testing 模板测试

新增 **Stencil testing** 场景，窗口请求 8 位模板缓冲，两个立方体使用 `resources/textures/container2.png`，具有纯黄色描边，支持 WASD、右键和滚轮。

1. 清除模板为 0。
2. 正常绘制带贴图的立方体：`glStencilFunc(GL_ALWAYS, 1, 0xff)`，`glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE)`。模板失败、深度失败时保留原值，两者都通过时把模板写为 1。
3. 放大物体绘制黄色外壳：关闭模板写入，用 `GL_NOTEQUAL` 与参考值 1 比较，只保留物体以外的轮廓。描边默认关闭深度测试。

`Stage` 的白色预览显示第一遍物体写入 1 的区域（几何覆盖预览，并非读取模板纹理）。关闭模板写入后，此预览仍显示物体，但实际模板保持 0；通过最终描边观察区别。

- 改 `Outline stencil function` 为 EQUAL：黄色填入原物体内部；ALWAYS 或关闭模板测试：黄色填满放大的轮廓；NEVER：不画黄色。
- 关闭 `Write stencil in object pass`：模板保持 0，默认 NOTEQUAL 在整个外壳上通过。
- `Reference` 是第二遍比较的参考值。`Compare bit mask` 对参考值和已存值分别按位与；设为 0 后两边永远相等，NOTEQUAL 全部失败。
- 模板写入掩码决定哪些位允许修改，比较掩码决定哪些位参与判断；两者用途不同。
- `Outline thickness` 控制模型放大比例，并非固定屏幕像素宽度。`Depth test for outline` 可观察其他几何对外壳的深度遮挡。
- `Reset stencil settings` 恢复默认参数及相机。每帧清空模板，并恢复深度和模板状态，切换场景不会残留遮罩。

## Blending 混合

新增 **Blending** 场景，参考 [LearnOpenGL Blending](https://learnopengl.com/Advanced-OpenGL/Blending)：两个大理石箱子、金属地面，以及教程位置的五扇半透明窗户。额外放置三片草丛，避免与窗户共面。所有物体使用同一相机，支持 WASD、右键拖动和滚轮。

绘制依次执行：

1. 关闭混合，绘制箱子和地面，启用深度测试和深度写入。
2. 绘制 `grass.png`，默认丢弃 alpha < 0.1 的片元。留下的草叶正常写深度，透明区域不会遮挡后方物体。
3. 每帧根据当前相机到窗户放置位置的距离排序，以从远到近的顺序绘制 `window.png`，使用 `GL_SRC_ALPHA` / `GL_ONE_MINUS_SRC_ALPHA` 混合。保持深度测试，默认关闭窗户深度写入。透明纹理使用 `GL_CLAMP_TO_EDGE`，避免重复采样的边缘颜色渗漏。

`Window draw order` 支持 **Far to near**（默认）、**Near to far** 和 **Original placement order**，`Current window order` 展示实际顺序及距离。移动相机后排序实时更新，等距离窗户也不会被丢失。此处按物体距离排序与教程一致，并非逐像素排序；相交或复杂交叠的透明几何仍可能出现误差。

建议对比：关闭窗户混合观察不透明色块；切换从近到远并开启 `Write window depth`，观察远窗被近窗挡掉；关闭深度写入后比较不同顺序的颜色叠加。草丛可切换 `Discard transparent grass pixels` 并调节阈值，观察不丢弃时透明区域形成矩形遮挡。草丛与窗户可以分别隐藏。

所有混合函数、混合方程、深度和面剔除状态在绘制结束后恢复，不影响其他场景。

## Framebuffers 帧缓冲与后期处理

**Framebuffers** 使用独立的 `FramebufferScene.cpp`。第一遍把箱子与地面绘制到 FBO 的 RGB8 颜色纹理，使用 DEPTH24_STENCIL8 渲染缓冲附件提供深度测试；第二遍回到窗口帧缓冲，用全屏四边形采样颜色纹理并做后期处理。ImGui 在最后绘制，不受滤镜影响。

- `Post-processing`：原图、反色、亮度加权灰度、3×3 锐化、高斯模糊、边缘检测（取绝对值）、复古色调。
- `Effect strength`：混合原图与处理结果；0 为原图，1 为完整效果。
- `Kernel radius (pixels)`：调节卷积采样间距，按实际颜色纹理尺寸计算 UV 偏移。
- `Split view`：左半边原图、右半边处理结果，方便实时对比。
- 窗口缩放或高分屏尺寸变化时，自动重新分配颜色纹理与深度附件，并检查帧缓冲完整性。面板显示实际离屏分辨率。
- 保留 WASD、右键、滚轮相机控制，支持重置滤镜及相机。GPU 资源随场景销毁，绘制后恢复帧缓冲、视口和深度等状态。

## Cubemaps 立方体贴图

**Cubemaps** 使用 `resources/textures/skybox/` 中的六张图片，按 +X/-X/+Y/-Y/+Z/-Z 对应 right/left/top/bottom/front/back 加载，不做垂直翻转。天空盒移除相机平移，使用 `xyww` 将深度置于远平面，最后以 LEQUAL 且禁止深度写入绘制，因此不会覆盖前面的物体。

左侧立方体默认反射，右侧光滑球体默认折射。`Cube surface`、`Sphere surface` 独立切换 Reflection / Refraction；折射模式可分别调整材质折射率，默认玻璃约 1.52，水约 1.333，钻石约 2.42。物体使用世界空间位置与法线计算 `reflect` / `refract`，折射比为空气折射率 / 材质折射率。相机进入物体内部时反转法线及折射比，全内反射时回退到反射。

这是单表面的环境映射近似，不模拟玻璃厚度或出射表面，也不会映出另一个物体。WASD、右键和滚轮控制相机；面板可分别重置材质参数及相机。天空盒与物体共享同一份 cubemap，开启无缝采样，并在场景绘制后恢复深度和纹理状态。

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

冒烟测试逻辑集中在 `engine/src/SmokeTest.cpp`，由 `engine/include/SmokeTest.h` 声明接口；`main.cpp` 只负责初始化上下文和调用测试。测试使用隐藏 GLFW 窗口，轮流创建、绘制、销毁所有场景三轮，并检查 framebuffer 是否有预期输出及是否出现 OpenGL 错误：

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
- xmake 的 Assimp 产物位于 `build/xmake-assimp/<平台>/<架构>/<工具链>/<模式>/`，区分 MinGW/MSVC，不与主 CMake 构建混用。
- 模型预览场景使用 `engine/src/ModelScene.cpp` 与 `resources/shaders/model/`，通过 Assimp 导入静态模型并绘制基础颜色/漫反射贴图。
- `Assimp::Importer` 拥有 `ReadFile()` 返回的 `aiScene`；Importer 析构或再次导入后指针失效，渲染层须提前复制需要的顶点/索引/材质数据。
- 版本、上游提交和许可证说明见 `third_party/assimp/INTEGRATION.md`。

### Windows（Visual Studio 2019/2022 MSVC）

安装 Visual Studio 的“使用 C++ 的桌面开发”工作负载、Windows SDK 和 CMake ≥ 3.22，以下命令使用 x64 架构。主程序和 Assimp 须使用同一工具链，不能混用 MinGW/MSVC 库。

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

### Windows（MSYS2 MinGW64）

将 `C:\msys64\mingw64\bin` 和 xmake 加入 PATH，确认其中有 GCC、G++、CMake ≥ 3.22 和 Ninja。两套构建均从源码编译 Assimp，使用相同的 MinGW 工具链。

```powershell
# CMake；Debug 将两处 mingw-release 改为 mingw-debug
cmake --preset mingw-release
cmake --build --preset mingw-release
.\build\cmake-mingw-release\OpenGLLearn.exe

# xmake
xmake f -p windows -a x64 -m release --toolchain=mingw --mingw=C:\msys64\mingw64
xmake
xmake run OpenGLLearn
```

在本项目目录运行 `..\FluxPlayer\setup_env.ps1` 会配置当前项目使用 MinGW；随后可直接执行 `xmake`。切换工具链应重新运行 `xmake f`，CMake 应使用独立构建目录。

## 添加教程场景

1. 在 `resources/shaders/` 下按类别保存该场景的 `.vs` 和 `.fs` 文件。
2. 在 `engine/src/scenes/<场景名>Scene.cpp` 中实现一个 `Scene` 派生类，并导出创建函数，在 `scenes/SceneFactories.h` 中声明。共用网格、纹理和相机辅助代码位于 `scenes/SceneSupport.h/.cpp`。场景构造函数创建所需 GPU 资源，`render()` 绘制场景，析构时释放资源。
3. 在 `engine/src/Scenes.cpp` 的 `sceneInfos` 中增加面板名称，并在 `createScene()` 的工厂分支中调用该场景的创建函数；保持列表顺序与工厂索引一致。
4. 若场景需要相机输入，继承 `CameraScene` 或实现 `usesCamera()` 与 `input()`。
5. 将新场景源文件加入 `CMakeLists.txt`（xmake 自动包含 `scenes/*.cpp`），使用 CMake 和 xmake 构建，并运行 `--smoke-test` 验证。

`Mesh`、`Texture` 和 `Shader` 都遵循 RAII 管理 OpenGL 对象。销毁场景时必须仍有有效的 OpenGL 上下文，不能把 GPU 资源释放延迟到窗口销毁之后。

## 开发验证范围

当前在 macOS Apple Silicon 上验证 CMake、xmake 构建、全部场景的三轮 OpenGL 冒烟测试。Windows 已配置 MSVC 架构、CRT 与静态库链接，但尚未在 Windows 实机验证；不能将跨平台构建配置视为已经通过 Windows 测试。其他平台的 GLFW 系统依赖尚未在本项目验证。
