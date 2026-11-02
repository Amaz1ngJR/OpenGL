#pragma once
#include <memory>
#include <string>
#include <vector>

// 每帧由主循环提供；aspect 使用实际帧缓冲的宽高比，适配窗口缩放及高分屏。
struct SceneFrame {
    float time;       // 程序运行时间，单位为秒，用于动画
    float deltaTime;  // 本帧与上一帧的时间差，单位为秒
    float aspect;     // 帧缓冲宽度 / 高度
};

// 已经过 ImGui 捕获状态过滤的输入；场景无需直接依赖 GLFW。
struct SceneInput {
    bool forward = false, backward = false, left = false, right = false;
    float mouseX = 0, mouseY = 0, scroll = 0;
};

// 场景只持有自身的渲染资源；创建与销毁都必须发生在 GL 上下文有效期间。
class Scene {
public:
    virtual ~Scene() = default;
    virtual void render(const SceneFrame&) = 0;
    virtual void input(const SceneInput&, float) {}
    // 由主循环在 ImGui 窗口内调用，先修改参数，再在同一帧 render() 中上传。
    virtual void drawControls() {}
    virtual bool usesCamera() const { return false; }
};

struct SceneInfo { const char* name; };
// 列表顺序与工厂索引对应，供控制面板和冒烟测试共同使用。
const std::vector<SceneInfo>& availableScenes();
std::unique_ptr<Scene> createScene(int index, const std::string& root);
