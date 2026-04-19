# LearnOpenGL

按照 [LearnOpenGL](https://learnopengl-cn.github.io/) 教程一步步实现的 OpenGL demo 合集。每个 demo 单独一个目录，完全自包含，支持 **xmake** 和 **cmake** 双构建体系。

## 目录结构

```
OpenGL/
├── third_party/              # 第三方依赖（精简版）
│   ├── glfw-3.3.8/           # 窗口与输入
│   ├── glad/                 # OpenGL 函数加载
│   └── glm/                  # 数学库
├── resources/
│   └── textures/             # 共享纹理资源
├── 入门/
│   ├── 01_triangle_base/     # 最基础的三角形
│   ├── 02_triangle_uniform/  # 使用 uniform 动态传值
│   └── 03_transformations/   # 3D 变换 + 摄像机
├── 纹理/
│   ├── 01_textures_base/     # 基础纹理
│   └── 02_texture_combined/  # 多纹理混合
├── 光照/
│   ├── 01_phong/             # Phong 光照模型
│   └── 02_material/          # 材质系统
└── .gitignore
```

每个 demo 内部结构：

```
XX_demo_name/
├── include/       # 头文件（Shader.h, Camera.h, stb_image.h 按需）
├── src/           # 源文件（main.cpp, Shader.cpp, Camera.cpp 按需）
├── shader/        # GLSL 着色器（*.vs, *.fs）
├── xmake.lua      # xmake 构建配置
└── CMakeLists.txt # cmake 构建配置
```

## 环境要求

- **C++17** 编译器（macOS AppleClang / Windows MSVC / Linux g++）
- **xmake** 或 **cmake** ≥ 3.10
- OpenGL 3.3 Core Profile

目前仅在 **macOS (Apple Silicon)** 上验证通过，Windows 应能编译运行（Linux 暂不支持）。

## 构建与运行

每个 demo 独立构建、独立运行。以 `纹理/01_textures_base` 为例：

### 方式一：xmake

```bash
cd 纹理/01_textures_base
xmake                # 构建，输出到 build/
cd build && ./TexturesBase
# 或者直接：
xmake run
```

### 方式二：cmake

```bash
cd 纹理/01_textures_base
cmake -B build
cmake --build build
cd build && ./TexturesBase
```

> ⚠️ 必须在 `build/` 目录下运行二进制文件，否则 shader 和纹理的相对路径找不到。

## 各 demo 依赖关系

| Demo              | Shader | Camera | 纹理               |
|-------------------|--------|--------|--------------------|
| 01_triangle_base  | ✓      |        |                    |
| 02_triangle_uniform | ✓    |        |                    |
| 03_transformations | ✓     | ✓      | awesomeface, container |
| 01_textures_base  | ✓      |        | awesomeface        |
| 02_texture_combined | ✓    |        | awesomeface, container |
| 01_phong          | ✓      | ✓      |                    |
| 02_material       | ✓      | ✓      |                    |

## 新增 demo

在对应分类目录下新建文件夹，复制一个已有 demo 作为模板，修改 `xmake.lua` / `CMakeLists.txt` 中的 `project(...)` 名即可。

## 操作说明

- **ESC** 退出程序
- **WASD** 移动摄像机（仅使用 Camera 的 demo）
- **鼠标** 控制视角（仅使用 Camera 的 demo）
- **滚轮** 缩放视野（仅使用 Camera 的 demo）
