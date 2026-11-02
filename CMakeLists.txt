cmake_minimum_required(VERSION 3.16)
project(OpenGLLearn LANGUAGES C CXX)

# 统一引擎只生成一个可执行文件；各教程场景在运行时由 ImGui 选择。

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(THIRD_PARTY_DIR ${ROOT_DIR}/third_party)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/glfw-3.3.8 ${CMAKE_CURRENT_BINARY_DIR}/glfw)

add_library(glad STATIC ${THIRD_PARTY_DIR}/glad/src/glad.c)
target_include_directories(glad PUBLIC ${THIRD_PARTY_DIR}/glad/include)

add_executable(OpenGLLearn
    engine/src/main.cpp
    engine/src/Shader.cpp
    engine/src/Camera.cpp
    engine/src/Scenes.cpp
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(OpenGLLearn PRIVATE
    engine/include
    ${THIRD_PARTY_DIR}/glfw-3.3.8/include
    ${THIRD_PARTY_DIR}/glad/include
    ${THIRD_PARTY_DIR}/glm
    ${THIRD_PARTY_DIR}/imgui
    ${THIRD_PARTY_DIR}/imgui/backends
    ${THIRD_PARTY_DIR}
)
# 将仓库根路径编译进程序，使着色器和纹理不依赖启动工作目录。
target_compile_definitions(OpenGLLearn PRIVATE OPENGL_LEARN_ROOT="${ROOT_DIR}")
target_link_libraries(OpenGLLearn PRIVATE glfw glad)
if(APPLE)
    target_link_libraries(OpenGLLearn PRIVATE "-framework OpenGL")
elseif(WIN32)
    target_link_libraries(OpenGLLearn PRIVATE opengl32 gdi32)
endif()
set_target_properties(OpenGLLearn PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
