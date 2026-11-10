#include "Scene.h"
#include "ModelScene.h"
#include "FileDialog.h"
#include "SmokeTest.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr int WindowWidth = 1100;
constexpr int WindowHeight = 720;

// GLFW 和 ImGui 只由这一处入口维护；章节场景不创建窗口，也不安装回调。
// 输入由主循环集中采集：鼠标回调只缓存位移，UI 可随时切换场景。
struct InputState {
    double lastX = 0, lastY = 0;
    bool firstMouse = true;
    float mouseX = 0, mouseY = 0, scroll = 0;
};
void cursorCallback(GLFWwindow* window, double x, double y) {
    auto& input = *static_cast<InputState*>(glfwGetWindowUserPointer(window));
    if (!input.firstMouse) {
        input.mouseX += static_cast<float>(x - input.lastX);
        input.mouseY += static_cast<float>(input.lastY - y);
    }
    input.lastX = x;
    input.lastY = y;
    input.firstMouse = false;
}
void scrollCallback(GLFWwindow* window, double, double y) {
    auto& input = *static_cast<InputState*>(glfwGetWindowUserPointer(window));
    input.scroll += static_cast<float>(y);
}

// 摄像机输入集中在主循环转发，场景本身不注册 GLFW 回调。
void run(GLFWwindow* window, InputState& state, const std::string& root) {
    int selected = 0;
    auto scene = createScene(0, root);
    std::string error;
    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const double now = glfwGetTime();
        const float deltaTime = std::min(static_cast<float>(now - previous), .1f);
        previous = now;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("LearnOpenGL Scenes");
        ImGui::TextUnformatted("Select one scene to render:");
        if (ImGui::Button("Load local model...")) {
            try {
                const std::string path = selectModelFile();
                if (!path.empty()) {
                    scene = createModelScene(root, path);
                    selected = 1;
                    error.clear();
                    state.firstMouse = true;
                    state.mouseX = state.mouseY = state.scroll = 0;
                }
            } catch (const std::exception& e) {
                error = e.what();
                std::cerr << error << '\n';
            }
            // 系统对话框可能泵入鼠标事件；取消或加载失败也不沿用这些位移。
            state.firstMouse = true;
            state.mouseX = state.mouseY = state.scroll = 0;
        }
        const auto& options = availableScenes();
        for (int index = 0; index < static_cast<int>(options.size()); ++index) {
            if (ImGui::RadioButton(options[index].name, selected == index) && selected != index) {
                try {
                    // 加载失败时保留旧场景，用户仍可操作面板。
                    auto next = createScene(index, root);
                    scene = std::move(next);
                    selected = index;
                    error.clear();
                    state.firstMouse = true;
                    state.mouseX = state.mouseY = state.scroll = 0;
                } catch (const std::exception& e) {
                    error = e.what();
                    std::cerr << error << '\n';
                }
            }
        }
        ImGui::Separator();
        ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
        if (scene->usesCamera()) {
            ImGui::TextUnformatted("WASD: move | RMB + drag: look");
            ImGui::TextUnformatted("Scroll: zoom | Esc: exit");
        }
        if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
        // 场景自己声明参数控件，主循环不必了解纹理混合或材质的具体实现。
        scene->drawControls();
        ImGui::End();

        // 只在控制面板外采集 3D 相机输入，避免 WASD 与 UI 键盘操作冲突。
        SceneInput input;
        const auto& io = ImGui::GetIO();
        if (scene->usesCamera() && !io.WantCaptureKeyboard && glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            input.forward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
            input.backward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
            input.left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
            input.right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        }
        if (scene->usesCamera() && !io.WantCaptureMouse && glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            // 不锁住光标，随时能返回面板切换场景。
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                input.mouseX = state.mouseX;
                input.mouseY = state.mouseY;
            }
            input.scroll = state.scroll;
        }
        state.mouseX = state.mouseY = state.scroll = 0;
        scene->input(input, deltaTime);
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0) {
            glViewport(0, 0, width, height);
            glClearColor(.12f, .14f, .18f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            scene->render({static_cast<float>(now), deltaTime, static_cast<float>(width) / height});
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    // 场景析构先于 ImGui 和 GLFW，保证删除 GPU 对象时上下文仍然有效。
}
}

// 唯一应用入口：初始化窗口/GL/ImGui、派发输入、驱动场景和 GUI 帧循环。
int main(int argc, char** argv) {
    const bool smoke = argc >= 2 && std::string(argv[1]) == "--smoke-test";
    std::string modelPath;
    if (smoke && argc == 4 && std::string(argv[2]) == "--model") modelPath = argv[3];
    else if (argc != 1 && !(smoke && argc == 2)) {
        std::cerr << "Usage: OpenGLLearn [--smoke-test [--model <path>]]\n";
        return 1;
    }
    glfwSetErrorCallback([](int, const char* message) { std::cerr << "GLFW: " << message << '\n'; });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    if (smoke) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(WindowWidth, WindowHeight, "LearnOpenGL - Scene Browser", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    // 先使上下文当前，再加载 OpenGL 函数；后续场景构造会立即创建 GPU 资源。
    glfwSwapInterval(smoke ? 0 : 1);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    InputState input;
    glfwSetWindowUserPointer(window, &input);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);
    IMGUI_CHECKVERSION();
    // ImGui GLFW backend 接管基础回调；自定义鼠标回调通过后端链式转发。
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    const bool glfwReady = ImGui_ImplGlfw_InitForOpenGL(window, true);
    const bool rendererReady = glfwReady && ImGui_ImplOpenGL3_Init("#version 330 core");
    int result = 0;
    try {
        if (!rendererReady) throw std::runtime_error("Failed to initialize ImGui backends");
        if (smoke) smokeTest(window, OPENGL_LEARN_ROOT, modelPath);
        else run(window, input, OPENGL_LEARN_ROOT);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    if (rendererReady) ImGui_ImplOpenGL3_Shutdown();
    if (glfwReady) ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
