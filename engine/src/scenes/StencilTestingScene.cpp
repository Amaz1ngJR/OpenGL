#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
class StencilTestingScene final : public CameraScene {
public:
    explicit StencilTestingScene(const std::string& root)
        : shader_(loadShader(root, "stencil-testing", "scene")), cube_(cubeVertices(false), {3,2}),
          container_(root + "/resources/textures/container2.png") {}
    void render(const SceneFrame& frame) override {
        // 深度和模板是独立测试；保存比较、操作及写掩码，结束后还原上下文状态。
        GLboolean depth = glIsEnabled(GL_DEPTH_TEST), stencil = glIsEnabled(GL_STENCIL_TEST), mask;
        GLint depthFunc, func, ref, valueMask, writeMask, fail, zfail, pass, clear;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_STENCIL_FUNC, &func); glGetIntegerv(GL_STENCIL_REF, &ref);
        glGetIntegerv(GL_STENCIL_VALUE_MASK, &valueMask); glGetIntegerv(GL_STENCIL_WRITEMASK, &writeMask);
        glGetIntegerv(GL_STENCIL_FAIL, &fail); glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &zfail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &pass); glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clear);
        // glClear 也受写掩码影响，先允许写全部 8 位，再将模板缓冲清为 0。
        glStencilMask(0xff); glClearStencil(0); glClear(GL_STENCIL_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glEnable(GL_STENCIL_TEST);
        shader_->use();
        shader_->setMat4("view", camera_.GetViewMatrix());
        shader_->setMat4("projection", projection(frame));
        // 物体稍向右放，给左侧控制面板留空间；同一几何绘制两次，第二遍放大成外壳。
        auto draw = [&](float scale, glm::vec3 color) {
            shader_->setVec3("color", color);
            for (auto position : {glm::vec3(.3f,0,0), glm::vec3(1.1f,.15f,-1.2f)}) {
                auto model = glm::translate(glm::mat4(1), position);
                model = glm::rotate(model, glm::radians(25.f), glm::vec3(0,1,0));
                shader_->setMat4("model", glm::scale(model, glm::vec3(scale)));
                cube_.draw();
            }
        };
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilMask(writeStencil_ ? 0xff : 0x00);
        // 三种操作依次对应模板失败、模板通过但深度失败、两者都通过；仅最后一种写 ref=1。
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        // 第一遍绘制箱子贴图并写入模板；掩码预览只显示白色，便于观察写入区域。
        container_.bind(0);
        shader_->setInt("surfaceTexture", 0);
        shader_->setInt("useTexture", stage_ != 1);
        draw(1, glm::vec3(1));
        if (stage_ != 1) {
            // 第二遍使用纯色放大外壳，不能把箱子贴图带到描边上。
            shader_->setInt("useTexture", false);
            glStencilMask(0); // 第二遍只读模板，不再修改第一遍记录的轮廓区域。
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
            const GLenum functions[] = {GL_NOTEQUAL, GL_EQUAL, GL_ALWAYS, GL_NEVER};
            // 默认 NOTEQUAL 排除模板为 1 的本体区域，只留下放大外壳超出的那圈描边。
            // compareMask_ 是比较位掩码，不等于控制写入的 glStencilMask。
            glStencilFunc(functions[function_], reference_, compareMask_);
            if (!testStencil_) glDisable(GL_STENCIL_TEST);
            if (!outlineDepth_) glDisable(GL_DEPTH_TEST);
            draw(1.f + thickness_, glm::vec3(1,.65f,.05f));
        }
        glStencilMask(writeMask); glClearStencil(clear);
        glStencilFunc(func, ref, valueMask); glStencilOp(fail,zfail,pass);
        if (stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        glDepthMask(mask); glDepthFunc(depthFunc);
        if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
    void drawControls() override {
        ImGui::TextWrapped("1. Clear stencil to 0. 2. Textured boxes write 1 on depth pass. 3. Enlarged yellow objects draw where stencil != 1.");
        const char* stages[] = {"Final outline", "Mask preview (white = 1)"};
        ImGui::Combo("Stage", &stage_, stages, 2);
        ImGui::Checkbox("Write stencil in object pass", &writeStencil_);
        ImGui::Checkbox("Stencil test in outline pass", &testStencil_);
        const char* funcs[] = {"GL_NOTEQUAL", "GL_EQUAL", "GL_ALWAYS", "GL_NEVER"};
        ImGui::Combo("Outline stencil function", &function_, funcs, 4);
        ImGui::SliderInt("Reference", &reference_, 0, 1);
        ImGui::SliderInt("Compare bit mask", &compareMask_, 0, 255);
        ImGui::SliderFloat("Outline thickness", &thickness_, .01f, .3f);
        ImGui::Checkbox("Depth test for outline", &outlineDepth_);
        ImGui::TextWrapped("Comparison uses (reference & mask) and (stored & mask). Mask 0 makes them equal everywhere. Write mask controls which bits can change; it does not control comparison.");
        ImGui::TextWrapped("glStencilOp(sfail, dpfail, dppass): KEEP, KEEP, REPLACE here. Only fragments passing both tests write reference 1. Outline uses KEEP for all outcomes.");
        ImGui::TextWrapped("Try EQUAL to paint the interior; ALWAYS or disabling stencil fills the enlarged silhouette. Disable writes: NOTEQUAL passes everywhere. Outline depth OFF keeps the shell visible through other objects.");
        if (ImGui::Button("Reset stencil settings")) {
            stage_=function_=0; reference_=1; compareMask_=255;
            writeStencil_=testStencil_=true; outlineDepth_=false; thickness_=.08f;
            camera_=Camera(glm::vec3(0,0,3));
        }
    }
private:
    std::unique_ptr<Shader> shader_;
    Mesh cube_;
    Texture container_;
    bool writeStencil_=true, testStencil_=true, outlineDepth_=false;
    int stage_=0, function_=0, reference_=1, compareMask_=255;
    float thickness_=.08f;
};

} // namespace

std::unique_ptr<Scene> createStencilTestingScene(const std::string& root) {
    return std::make_unique<StencilTestingScene>(root);
}
