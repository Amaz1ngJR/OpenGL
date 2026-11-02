#include "Scene.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

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

// 通过真实 ImGui 鼠标事件切换光照控件的首个复选框，覆盖暂停与恢复路径。
void toggleLightPause(Scene& scene) {
    ImVec2 checkbox;
    for (int step = 0; step < 4; ++step) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        if (step > 0) ImGui::GetIO().AddMousePosEvent(checkbox.x, checkbox.y);
        if (step == 2) ImGui::GetIO().AddMouseButtonEvent(0, true);
        if (step == 3) ImGui::GetIO().AddMouseButtonEvent(0, false);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(360, 500));
        ImGui::Begin("Pause control test");
        checkbox = ImGui::GetCursorScreenPos();
        checkbox.x += 5;
        checkbox.y += 5;
        scene.drawControls();
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

// 在真实 OpenGL 上下文下反复创建/绘制/销毁所有场景，检查 GL 错误和非空画面。
void smokeTest(GLFWwindow* window, const std::string& root) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) throw std::runtime_error("No drawable framebuffer");
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    for (int round = 0; round < 3; ++round) {
        for (int index = 0; index < static_cast<int>(availableScenes().size()); ++index) {
            auto scene = createScene(index, root);
            glViewport(0, 0, width, height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            scene->render({1.0f, .016f, static_cast<float>(width) / height});
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            size_t colored = 0;
            for (size_t p = 0; p < pixels.size(); p += 4)
                if (pixels[p] || pixels[p + 1] || pixels[p + 2]) ++colored;
            if ((index == 0 && colored != 0) || (index != 0 && colored == 0))
                throw std::runtime_error(std::string("Unexpected framebuffer: ") + availableScenes()[index].name);
            if (index == 5) {
                // 先在 time=1 时绘制并记下光源矩阵与颜色。
                scene->render({1.0f, .016f, static_cast<float>(width) / height});
                GLint program = 0;
                glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                const GLint modelLocation = glGetUniformLocation(program, "model");
                const GLint colorLocation = glGetUniformLocation(program, "lightColor");
                GLfloat previousModel[16]{}, previousColor[3]{};
                glGetUniformfv(program, modelLocation, previousModel);
                glGetUniformfv(program, colorLocation, previousColor);

                toggleLightPause(*scene);
                for (float time : {2.0f, 3.0f}) {
                    scene->render({time, .016f, static_cast<float>(width) / height});
                    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                    GLfloat model[16]{}, color[3]{};
                    glGetUniformfv(program, glGetUniformLocation(program, "model"), model);
                    glGetUniformfv(program, glGetUniformLocation(program, "lightColor"), color);
                    for (int component = 0; component < 16; ++component)
                        if (std::abs(model[component] - previousModel[component]) > .001f)
                            throw std::runtime_error("Paused light position changed");
                    for (int component = 0; component < 3; ++component)
                        if (std::abs(color[component] - previousColor[component]) > .001f)
                            throw std::runtime_error("Paused light color changed");
                }
                // 取消暂停后时间恢复推进，模型矩阵中的光源位置应继续绕圈。
                toggleLightPause(*scene);
                scene->render({4.0f, .016f, static_cast<float>(width) / height});
                glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                GLfloat resumedModel[16]{}, resumedColor[3]{};
                glGetUniformfv(program, glGetUniformLocation(program, "model"), resumedModel);
                glGetUniformfv(program, glGetUniformLocation(program, "lightColor"), resumedColor);
                // 暂停两秒不计入动画；恢复这一帧时有效动画时间应是 2 秒，而不是全局 4 秒。
                if (std::abs(resumedModel[12] - std::sin(2.f * .8f) * 2.f) > .001f ||
                    std::abs(resumedModel[14] - std::cos(2.f * .8f) * 2.f) > .001f ||
                    std::abs(resumedColor[0] - (.5f + .5f * std::sin(2.f * 2.f))) > .001f ||
                    std::abs(resumedColor[1] - (.5f + .5f * std::sin(2.f * .7f))) > .001f ||
                    std::abs(resumedColor[2] - (.5f + .5f * std::sin(2.f * 1.3f))) > .001f)
                    throw std::runtime_error("Light animation did not resume from paused time");
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Scene smoke test");
            for (int option = 0; option < static_cast<int>(availableScenes().size()); ++option)
                ImGui::RadioButton(availableScenes()[option].name, option == index);
            scene->drawControls();
            ImGui::End();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            // 删除当前绑定的程序前解绑，避免 ImGui 下一帧恢复已删除的 program。
            glUseProgram(0);
            glBindVertexArray(0);
            scene.reset();
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error(std::string("OpenGL error: ") + availableScenes()[index].name);
            std::cout << "PASS " << round + 1 << ": " << availableScenes()[index].name << '\n';
        }
    }
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
    const bool smoke = argc == 2 && std::string(argv[1]) == "--smoke-test";
    if (argc > 1 && !smoke) {
        std::cerr << "Usage: OpenGLLearn [--smoke-test]\n";
        return 1;
    }
    glfwSetErrorCallback([](int, const char* message) { std::cerr << "GLFW: " << message << '\n'; });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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
        if (smoke) smokeTest(window, OPENGL_LEARN_ROOT);
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
