#include "SceneFactories.h"
#include "SceneSupport.h"
#include <algorithm>
#include <cmath>

using namespace sceneSupport;
namespace {
class DepthTestingScene final : public CameraScene {
public:
    explicit DepthTestingScene(const std::string& root)
        : shader_(loadShader(root, "depth-testing", "scene")),
          card_(std::vector<float>{-1,-1,0,0,0, 1,-1,0,1,0, 1,1,0,1,1, -1,1,0,0,1}, {3,2}, {0,1,2,0,2,3}),
          cube_(cubeVertices(false), {3,2}),
          floor_(std::vector<float>{
              // 地面 y=-0.5，与边长 1 的箱子底部对齐；UV 0..2 使金属纹理重复。
               5,-.5f, 5, 2,0,  -5,-.5f, 5, 0,0,  -5,-.5f,-5, 0,2,
               5,-.5f, 5, 2,0,  -5,-.5f,-5, 0,2,   5,-.5f,-5, 2,2
          }, {3,2}),
          marble_(root + "/resources/textures/marble.jpg"),
          metal_(root + "/resources/textures/metal.png") {
        shader_->use();
        shader_->setInt("surfaceTexture", 0);
    }
    void render(const SceneFrame& frame) override {
        // 深度比较和写入是上下文状态，保存/恢复，不能泄露到下一帧清屏和其他场景。
        DepthState previous;
        glDepthFunc(depthFunctions_[depthFunction_]);
        glDepthMask(writeDepth_ ? GL_TRUE : GL_FALSE);
        if (testDepth_) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
        shader_->use();
        shader_->setMat4("view", camera_.GetViewMatrix());
        shader_->setMat4("projection", glm::perspective(glm::radians(camera_.Zoom), frame.aspect, nearPlane_, farPlane_));
        shader_->setFloat("nearPlane", nearPlane_);
        shader_->setFloat("farPlane", farPlane_);
        shader_->setInt("displayMode", displayMode_);
        shader_->setInt("solidColor", false);
        // Optional reference geometry shares the scene camera and projection.
        marble_.bind(0); // Keep the sampler complete even during solid-color reference draws.
        if (referenceDepth_) drawCards(true);
        if (floorFirst_) drawFloor();
        drawCubes();
        if (!floorFirst_) drawFloor();
        drawCards(false);
    }
    void drawControls() override {
        ImGui::Separator();
        ImGui::TextUnformatted("Depth buffer settings");
        ImGui::Checkbox("Gray card depth reference", &referenceDepth_);
        // 卡片是深度比较的教学辅助几何，颜色仅用于区分身份，不代表深度值或光照。
        if (ImGui::CollapsingHeader("What are the colored and gray cards?", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("These are depth-test demonstration surfaces, not textures, lights or a depth heatmap. Their colors identify three different depth relationships.");
            ImGui::BulletText("Gray: stored reference depth.");
            ImGui::BulletText("Orange: closer than its gray card (incoming < stored).");
            ImGui::BulletText("Green: exactly replays its gray card (incoming == stored).");
            ImGui::BulletText("Blue: farther than its gray card (incoming > stored).");
            ImGui::TextWrapped("When enabled, gray cards are drawn first with GL_LESS and depth writes ON. Colored cards are drawn later using the selected depth settings. A rejected colored fragment leaves the earlier color visible; it does not erase that pixel.");
            // 表格说明的是单个彩色片元与对应灰卡的比较，不能保证最终画面不被其他几何覆盖。
            if (ImGui::BeginTable("Card depth comparison", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Function");
                ImGui::TableSetupColumn("Orange <");
                ImGui::TableSetupColumn("Green =");
                ImGui::TableSetupColumn("Blue >");
                ImGui::TableHeadersRow();
                const char* rows[][4] = {
                    {"LESS", "Pass", "Fail", "Fail"},
                    {"LEQUAL", "Pass", "Pass", "Fail"},
                    {"EQUAL", "Fail", "Pass", "Fail"},
                    {"NOTEQUAL", "Pass", "Fail", "Pass"},
                    {"GREATER", "Fail", "Fail", "Pass"},
                    {"GEQUAL", "Fail", "Pass", "Pass"},
                    {"ALWAYS", "Pass", "Pass", "Pass"},
                    {"NEVER", "Fail", "Fail", "Fail"}
                };
                for (const auto& row : rows) {
                    ImGui::TableNextRow();
                    for (int column = 0; column < 4; ++column) {
                        ImGui::TableSetColumnIndex(column);
                        ImGui::TextUnformatted(row[column]);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::TextWrapped("Table assumes the default camera and an overlapping pixel whose stored depth still comes from that card's gray reference. Cubes, floor, overlapping cards and depth writes can change the stored depth or final color. Moving behind the cards can reverse near/far relationships.");
            ImGui::TextWrapped("Try Reset camera, keep gray references ON, then compare LESS, EQUAL and GREATER in Textured mode. Turning references OFF removes the gray pre-pass, but colored cards remain as ordinary scene geometry.");
            ImGui::TextWrapped("Gray references write depth independently, even when the scene's Write depth buffer checkbox is OFF. Linear depth display replaces the identifying colors with depth grayscale.");
        }
        ImGui::Checkbox("Reverse colored card order", &reverseCards_);
        ImGui::TextWrapped("Colored card draw order: %s", reverseCards_ ? "Blue -> Green -> Orange" : "Orange -> Green -> Blue");
        // 解释操作影响的是提交顺序而非空间位置，避免误以为勾选会移动卡片。
        if (ImGui::CollapsingHeader("What does reversing card order do?", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("Only reverses the three colored cards' draw order, not their positions. Gray references, cubes and floor keep their own order.");
            ImGui::TextWrapped("With GL_LESS and depth writes ON, the closest passing fragment normally wins regardless of draw order.");
            ImGui::TextWrapped("With depth testing OFF or GL_ALWAYS, later cards overwrite earlier ones where they overlap. With depth writes OFF, order can also matter if multiple fragments pass the existing depth test.");
            ImGui::TextWrapped("No visible change is expected where cards do not overlap or only one card passes the depth comparison. Try GL_ALWAYS and compare the overlapping areas.");
        }
        ImGui::Checkbox("Enable depth test", &testDepth_);
        // GL_LESS 是默认规则：新片元深度小于已存深度才通过；GL_ALWAYS 无条件通过。
        // EQUAL/NOTEQUAL 比较缓冲中的精确深度，浮点误差和量化会影响相等测试。
        // GEQUAL/GREATER 不能单独构成反向 Z：本场景仍采用常规投影、深度清除值 1。
        const char* functions[] = {"GL_LESS", "GL_LEQUAL", "GL_EQUAL", "GL_NOTEQUAL",
                                  "GL_GREATER", "GL_GEQUAL", "GL_ALWAYS", "GL_NEVER"};
        if (testDepth_) ImGui::Combo("Depth function", &depthFunction_, functions, IM_ARRAYSIZE(functions));
        ImGui::Checkbox("Write depth buffer", &writeDepth_);
        ImGui::Checkbox("Draw floor first", &floorFirst_);
        const char* explanations[] = {
            "incoming < stored: closer fragments pass; equal fragments fail.",
            "incoming <= stored: closer AND equal fragments pass.",
            "incoming == stored: only exactly equal depth passes.",
            "incoming != stored: equal depth fails; closer and farther pass.",
            "incoming > stored: farther fragments pass.",
            "incoming >= stored: farther AND equal fragments pass.",
            "Always pass. Depth is still written if testing and writing are enabled.",
            "Never pass. Neither color nor depth is written."
        };
        ImGui::TextWrapped("%s", explanations[depthFunction_]);
        ImGui::TextWrapped("Writing OFF keeps the stored depth unchanged, but fragments still compare against it. Gray references, when enabled, write depth independently before the scene.");
        const char* modes[] = {"Textured", "Linear depth / far plane"};
        ImGui::Combo("Display", &displayMode_, modes, IM_ARRAYSIZE(modes));
        // 保证 0 < near < far，不能生成奇异透视矩阵或在线性化公式中除零。
        {
            ImGui::SliderFloat("Near plane", &nearPlane_, .01f, 2.f, "%.2f");
            ImGui::SliderFloat("Far plane", &farPlane_, 3.f, 100.f, "%.1f");
        }
        if (ImGui::Button("Reset depth settings")) {
            testDepth_ = writeDepth_ = true;
            floorFirst_ = false;
            depthFunction_ = displayMode_ = 0;
            nearPlane_ = .1f; farPlane_ = 100.f;
            referenceDepth_ = true; reverseCards_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) camera_ = Camera(glm::vec3(0,0,3));
        ImGui::TextWrapped("Depth clear value: 1.0. Disable testing or writing, then change draw order to compare occlusion.");
        if (!testDepth_) ImGui::TextWrapped("Depth writes also stop when GL_DEPTH_TEST is disabled, even if the write checkbox is on.");
        if (!referenceDepth_ && testDepth_ && (depthFunction_ == 2 || depthFunction_ == 4 || depthFunction_ == 5 || depthFunction_ == 7))
            ImGui::TextWrapped("With depth cleared to 1.0, this function may reject every fragment. A blank scene can be expected.");
    }
private:
    // RAII 保证异常退出 render() 时也恢复写入掩码，避免下一帧 glClear 无法清深度。
    struct DepthState {
        GLboolean enabled = glIsEnabled(GL_DEPTH_TEST), mask = GL_TRUE;
        GLint function = GL_LESS;
        DepthState() {
            glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);
            glGetIntegerv(GL_DEPTH_FUNC, &function);
        }
        ~DepthState() {
            glDepthMask(mask);
            glDepthFunc(static_cast<GLenum>(function));
            if (enabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        }
    };
    // Green replays the identical model and vertex data, so EQUAL compares the
    // same rasterized depth rather than merely approximately coplanar surfaces.
    void drawCards(bool reference) {
        shader_->setInt("solidColor", true);
        if (reference) {
            glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        }
        const glm::vec3 colors[] = {{1,.35f,.08f},{.1f,.9f,.3f},{.1f,.45f,1}};
        for (int n=0; n<3; ++n) {
            int i = !reference && reverseCards_ ? 2-n : n;
            float z = -1.5f + (reference ? 0.f : (.3f - i*.3f));
            auto model = glm::translate(glm::mat4(1), glm::vec3(.2f+i*.6f,.55f,z));
            shader_->setMat4("model", glm::scale(model, glm::vec3(.43f,.65f,1)));
            shader_->setVec3("color", reference ? glm::vec3(.28f) : colors[i]);
            card_.draw();
        }
        shader_->setInt("solidColor", false);
        if (reference) {
            glDepthFunc(depthFunctions_[depthFunction_]);
            glDepthMask(writeDepth_ ? GL_TRUE : GL_FALSE);
            if (!testDepth_) glDisable(GL_DEPTH_TEST);
        }
    }
    void drawFloor() {
        metal_.bind(0);
        shader_->setMat4("model", glm::mat4(1));
        floor_.draw();
    }
    void drawCubes() {
        marble_.bind(0);
        shader_->setMat4("model", glm::translate(glm::mat4(1), glm::vec3(-1,0,-1)));
        cube_.draw();
        shader_->setMat4("model", glm::translate(glm::mat4(1), glm::vec3(2,0,0)));
        cube_.draw();
    }
    inline static constexpr GLenum depthFunctions_[] = {
        GL_LESS, GL_LEQUAL, GL_EQUAL, GL_NOTEQUAL, GL_GREATER, GL_GEQUAL, GL_ALWAYS, GL_NEVER
    };
    std::unique_ptr<Shader> shader_;
    Mesh card_;
    bool referenceDepth_ = true, reverseCards_ = false;
    Mesh cube_, floor_;
    Texture marble_, metal_;
    bool testDepth_ = true, writeDepth_ = true, floorFirst_ = false;
    int depthFunction_ = 0, displayMode_ = 0;
    float nearPlane_ = .1f, farPlane_ = 100.f;
};

} // namespace

std::unique_ptr<Scene> createDepthTestingScene(const std::string& root) {
    return std::make_unique<DepthTestingScene>(root);
}
