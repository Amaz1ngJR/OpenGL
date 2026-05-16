# ==============================================================================
# CMakeLists.txt —— CMake 构建脚本
# ------------------------------------------------------------------------------
# 作用：告诉 CMake 如何编译本项目（TriangleBase），包括：
#   - 编译哪些源码
#   - 依赖哪些第三方库（GLFW / GLAD / GLM）
#   - 链接哪些系统库（OpenGL 框架）
#   - 头文件搜索路径的设置
#
# 典型构建流程：
#   mkdir build && cd build
#   cmake ..
#   cmake --build .
# ==============================================================================

cmake_minimum_required(VERSION 3.10)        # 要求的最低 CMake 版本
project(TriangleBase)                       # 项目名（同时作为默认的可执行目标名）
set(CMAKE_CXX_STANDARD 17)                  # 使用 C++17 标准

# ---- 第三方库根目录 ----
# 本项目位于 入门/01_triangle_base/，第三方库统一放在仓库根下的 third_party
set(ROOT_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(THIRD_PARTY_DIR ${ROOT_DIR}/third_party)

# ------------------------------------------------------------------------------
# GLFW —— 跨平台窗口与输入库
# 关闭文档 / 测试 / 示例的构建以减少编译时间
# ------------------------------------------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# 把第三方 GLFW 子目录加入构建树；第二个参数指定它的中间产物输出目录
add_subdirectory(${THIRD_PARTY_DIR}/glfw-3.3.8 ${CMAKE_CURRENT_BINARY_DIR}/glfw)

# ------------------------------------------------------------------------------
# GLAD —— OpenGL 函数指针加载器
# 只有一份 glad.c，直接编译为静态库
# ------------------------------------------------------------------------------
add_library(glad STATIC ${THIRD_PARTY_DIR}/glad/src/glad.c)
target_include_directories(glad PUBLIC ${THIRD_PARTY_DIR}/glad/include)

# ------------------------------------------------------------------------------
# 主程序目标
# ------------------------------------------------------------------------------
# 收集 src 目录下所有 .cpp 作为源码（注意：GLOB 不会在新增文件后自动 re-run cmake）
file(GLOB SOURCES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})

# 头文件搜索路径（PRIVATE：只对当前目标生效）
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include              # 本项目自己的头文件 (Shader.h)
    ${THIRD_PARTY_DIR}/glfw-3.3.8/include            # GLFW
    ${THIRD_PARTY_DIR}/glad/include                  # GLAD
    ${THIRD_PARTY_DIR}/glm                           # GLM (header-only)
)

# 链接 GLFW 与 GLAD
target_link_libraries(${PROJECT_NAME} glfw glad)

# ---- 按平台链接 OpenGL 实现 ----
if(APPLE)
    # macOS 上 OpenGL 以系统 Framework 形式存在
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
elseif(WIN32)
    # Windows 上需要 opengl32.lib 与 GDI
    target_link_libraries(${PROJECT_NAME} opengl32 gdi32)
endif()

# ---- 运行目录 / 调试目录设置 ----
# 因为 main.cpp 里用相对路径 "../shader/shader.vs" 加载 shader，
# 所以运行时的工作目录必须使得 "../shader/" 可达。
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}   # VS 调试时的工作目录
    RUNTIME_OUTPUT_DIRECTORY      ${CMAKE_CURRENT_BINARY_DIR}   # 可执行文件输出到 build 目录
)
