#include "Scene.h"
#include "Shader.h"
#include "Camera.h"
#include "imgui.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {
// 场景列表的顺序同时是 ImGui 选项的索引，新增场景时须同步注册工厂。
const std::vector<SceneInfo> sceneInfos = {
    {"No scene"}, {"Triangle"}, {"Uniform rectangle"}, {"Transformations"},
    {"Texture"}, {"Phong lighting + materials"}
};

// 网格对象独占 VAO/VBO/EBO，析构时释放 GPU 资源；不可复制以避免重复释放。
class Mesh {
public:
    Mesh(const std::vector<float>& vertices, std::initializer_list<int> attributes,
         const std::vector<unsigned int>& indices = {}) {
        int stride = 0;
        for (int size : attributes) stride += size;
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        if (!indices.empty()) {
            glGenBuffers(1, &ebo_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        }
        int offset = 0;
        unsigned int location = 0;
        for (int size : attributes) {
            glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                                  reinterpret_cast<void*>(static_cast<size_t>(offset) * sizeof(float)));
            glEnableVertexAttribArray(location++);
            offset += size;
        }
        count_ = static_cast<GLsizei>(indices.empty() ? vertices.size() / stride : indices.size());
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    ~Mesh() {
        glDeleteBuffers(1, &ebo_);
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
    }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    void draw() const {
        glBindVertexArray(vao_);
        if (ebo_) glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
        else glDrawArrays(GL_TRIANGLES, 0, count_);
    }
private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei count_ = 0;
};

// 纹理对象独占一个 OpenGL 纹理 ID；stb_image 的实现只在本文件编译一次。
class Texture {
public:
    explicit Texture(const std::string& filename, GLenum wrap = GL_REPEAT)
        : wrapS_(wrap == GL_MIRRORED_REPEAT ? 1 : 0), wrapT_(wrapS_) {
        int width = 0, height = 0, channels = 0;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) throw std::runtime_error("Cannot load texture: " + filename);
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        applySampling();
    }
    ~Texture() { glDeleteTextures(1, &id_); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    void bind(unsigned int unit) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, id_);
    }
    void drawControls(const char* label) {
        ImGui::PushID(label); // 每张纹理拥有独立控件 ID 和独立的采样参数。
        if (ImGui::TreeNode(label)) {
            // 纹理坐标超出 [0,1] 时的环绕规则：
            // GL_REPEAT：默认行为，重复纹理图像。
            // GL_MIRRORED_REPEAT：重复纹理，但每次重复都会镜像翻转。
            // GL_CLAMP_TO_EDGE：把坐标限制在 [0,1]，边缘之外重复边缘像素，产生拉伸效果。
            // GL_CLAMP_TO_BORDER：超出范围时使用用户指定的边缘颜色。
            const char* wraps[] = {"GL_REPEAT", "GL_MIRRORED_REPEAT", "GL_CLAMP_TO_EDGE", "GL_CLAMP_TO_BORDER"};
            // Minification（缩小）：一个屏幕像素覆盖多个纹理像素，需要选择或混合采样。
            // GL_NEAREST：选择最近的纹理像素；GL_LINEAR：在相邻纹理像素之间做线性插值。
            // mipmap 是逐级缩小的纹理图像；以下名称中，MIPMAP 前的部分决定层内采样，
            // MIPMAP 后的部分决定如何选择或混合层级：
            // GL_NEAREST_MIPMAP_NEAREST：选择最匹配的一层，在该层内使用最近邻采样。
            // GL_LINEAR_MIPMAP_NEAREST：选择最匹配的一层，在该层内使用线性插值。
            // GL_NEAREST_MIPMAP_LINEAR：在两个最匹配层级分别做最近邻采样，再对结果线性插值。
            // GL_LINEAR_MIPMAP_LINEAR：在两个最匹配层级分别做线性采样，再对结果线性插值（三线性过滤）。
            const char* minFilters[] = {"GL_NEAREST", "GL_LINEAR", "GL_NEAREST_MIPMAP_NEAREST",
                "GL_LINEAR_MIPMAP_NEAREST", "GL_NEAREST_MIPMAP_LINEAR", "GL_LINEAR_MIPMAP_LINEAR"};
            // Magnification（放大）只支持最近邻和线性插值，不支持 mipmap 过滤。
            const char* magFilters[] = {"GL_NEAREST", "GL_LINEAR"};
            bool changed = ImGui::Combo("Wrap S", &wrapS_, wraps, IM_ARRAYSIZE(wraps));
            changed |= ImGui::Combo("Wrap T", &wrapT_, wraps, IM_ARRAYSIZE(wraps));
            if (wrapS_ == 3 || wrapT_ == 3)
                changed |= ImGui::ColorEdit4("Border color", &borderColor_.x);
            // mipmap 只用于缩小过滤；放大过滤不能传入 GL_*_MIPMAP_*。
            changed |= ImGui::Combo("Min filter", &minFilter_, minFilters, IM_ARRAYSIZE(minFilters));
            changed |= ImGui::Combo("Mag filter", &magFilter_, magFilters, IM_ARRAYSIZE(magFilters));
            if (changed) applySampling();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
private:
    void applySampling() const {
        const GLenum wraps[] = {GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER};
        const GLenum minFilters[] = {GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST,
            GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR};
        const GLenum magFilters[] = {GL_NEAREST, GL_LINEAR};
        // 保存当前单元的绑定，防止编辑某张纹理时污染其他场景或纹理单元。
        GLint previous = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glBindTexture(GL_TEXTURE_2D, id_);
        // glTexParameteri 作用于当前绑定的纹理对象。S 对应 UV 的横向，T 对应纵向。
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wraps[wrapS_]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wraps[wrapT_]);
        // 边缘颜色仅在相应方向使用 GL_CLAMP_TO_BORDER 时参与采样。
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &borderColor_.x);
        // MIN 用于缩小，MAG 用于放大；mipmap 链已在加载图片后由 glGenerateMipmap 生成。
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilters[minFilter_]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilters[magFilter_]);
        glBindTexture(GL_TEXTURE_2D, previous);
    }
    GLuint id_ = 0;
    int wrapS_ = 0, wrapT_ = 0;
    int minFilter_ = 5, magFilter_ = 1;
    glm::vec4 borderColor_{1.0f, 0.0f, 1.0f, 1.0f};
};

// 统一着色器目录布局：resources/shaders/<分类>/<场景>/<文件名>.vs/.fs。
std::unique_ptr<Shader> loadShader(const std::string& root, const std::string& directory,
                                  const std::string& name = "shader") {
    const std::string prefix = root + "/resources/shaders/" + directory + "/" + name;
    return std::make_unique<Shader>((prefix + ".vs").c_str(), (prefix + ".fs").c_str());
}

// 每个面独立顶点，确保光照场景有正确的面法线和纹理坐标。
std::vector<float> cubeVertices(bool withNormals) {
    // 按每个面生成独立顶点，避免立方体边角处法线插值造成光照失真。
    const glm::vec3 corners[6][4] = {
        {{-.5f,-.5f,.5f}, {.5f,-.5f,.5f}, {.5f,.5f,.5f}, {-.5f,.5f,.5f}},
        {{.5f,-.5f,-.5f}, {-.5f,-.5f,-.5f}, {-.5f,.5f,-.5f}, {.5f,.5f,-.5f}},
        {{-.5f,.5f,.5f}, {.5f,.5f,.5f}, {.5f,.5f,-.5f}, {-.5f,.5f,-.5f}},
        {{-.5f,-.5f,-.5f}, {.5f,-.5f,-.5f}, {.5f,-.5f,.5f}, {-.5f,-.5f,.5f}},
        {{.5f,-.5f,.5f}, {.5f,-.5f,-.5f}, {.5f,.5f,-.5f}, {.5f,.5f,.5f}},
        {{-.5f,-.5f,-.5f}, {-.5f,-.5f,.5f}, {-.5f,.5f,.5f}, {-.5f,.5f,-.5f}}
    };
    const glm::vec2 uv[] = {{0,0}, {1,0}, {1,1}, {0,1}};
    const int order[] = {0,1,2,0,2,3};
    std::vector<float> vertices;
    for (const auto& face : corners) {
        const glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
        for (int index : order) {
            const auto& p = face[index];
            vertices.insert(vertices.end(), {p.x, p.y, p.z});
            if (withNormals) vertices.insert(vertices.end(), {normal.x, normal.y, normal.z});
            else vertices.insert(vertices.end(), {uv[index].x, uv[index].y});
        }
    }
    return vertices;
}

class EmptyScene final : public Scene {
public:
    void render(const SceneFrame&) override { glDisable(GL_DEPTH_TEST); }
};

// 入门场景共用绘制流程：普通模式插值顶点颜色，uniform 模式随时间更新绿色通道。
class TriangleScene final : public Scene {
public:
    TriangleScene(const std::string& root, bool uniform) : uniform_(uniform) {
        shader_ = loadShader(root, uniform ? "intro/uniform" : "intro/triangle");
        if (uniform) {
            // 用四个顶点和 EBO 绘制矩形，fragment shader 使用随时间变化的 uniform 颜色。
            mesh_ = std::make_unique<Mesh>(std::vector<float>{
                .5f,.5f,0, .5f,-.5f,0, -.5f,-.5f,0, -.5f,.5f,0
            }, std::initializer_list<int>{3}, std::vector<unsigned int>{0,1,3,1,2,3});
        } else {
            // 每顶点存储位置和 RGB，片元颜色由光栅化阶段插值。
            mesh_ = std::make_unique<Mesh>(std::vector<float>{
                .5f,-.5f,0, 1,0,0, -.5f,-.5f,0, 0,1,0, 0,.5f,0, 0,0,1
            }, std::initializer_list<int>{3,3});
        }
    }
    void render(const SceneFrame& frame) override {
        glDisable(GL_DEPTH_TEST);
        shader_->use();
        if (uniform_) {
            const float green = std::sin(frame.time) * .5f + .5f;
            glUniform4f(glGetUniformLocation(shader_->ID, "ourColor"), 0, green, 0, 1);
        }
        mesh_->draw();
    }
private:
    bool uniform_;
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Mesh> mesh_;
};

// 单纹理与混合纹理共用一个场景；所有采样与混合参数集中在同一面板。
class TextureScene final : public Scene {
public:
    explicit TextureScene(const std::string& root)
        : shader_(loadShader(root, "textures")),
          mesh_(std::vector<float>{
              .5f,.5f,0, 1,0,0, 1,1, .5f,-.5f,0, 0,1,0, 1,0,
              -.5f,-.5f,0, 0,0,1, 0,0, -.5f,.5f,0, 1,1,0, 0,1
          }, {3,3,2}, {0,1,3,1,2,3}),
          container_(root + "/resources/textures/container.jpg"),
          face_(root + "/resources/textures/awesomeface.png", GL_MIRRORED_REPEAT) {
        shader_->use();
        shader_->setInt("texture0", 0);
        shader_->setInt("texture1", 1);
    }
    void render(const SceneFrame&) override {
        glDisable(GL_DEPTH_TEST);
        shader_->use();
        shader_->setInt("combined", combined_);
        shader_->setInt("vertexTint", vertexTint_);
        shader_->setFloat("mixFactor", mixFactor_);
        shader_->setFloat("uvScale", uvScale_);
        container_.bind(0);
        face_.bind(1);
        mesh_.draw();
    }
    void drawControls() override {
        ImGui::Separator();
        ImGui::TextUnformatted("Texture settings");
        ImGui::Checkbox("Combine textures", &combined_);
        if (combined_) ImGui::SliderFloat("Texture mix", &mixFactor_, 0.0f, 1.0f, "%.2f");
        else ImGui::TextUnformatted("Single texture: awesomeface");
        ImGui::Checkbox("Vertex color tint", &vertexTint_);
        // 以纹理中心缩放 UV，超出 [0,1] 时才能清晰观察各种环绕方式。
        ImGui::SliderFloat("UV scale", &uvScale_, .1f, 20.0f, "%.2f");
        ImGui::TextWrapped("Use UV scale > 1 to compare wrap modes. Min filters affect shrinking; mag filters affect enlargement.");
        if (combined_) container_.drawControls("Container (texture0)");
        face_.drawControls("Awesomeface (texture1)");
    }
private:
    bool combined_ = true;
    bool vertexTint_ = false;
    float mixFactor_ = 0.8f;
    float uvScale_ = 3.0f;
    std::unique_ptr<Shader> shader_;
    Mesh mesh_;
    Texture container_, face_;
};

// 所有 3D 场景共用一份摄像机、输入处理和按窗口宽高比生成的透视投影。
class CameraScene : public Scene {
public:
    bool usesCamera() const override { return true; }
    void input(const SceneInput& input, float dt) override {
        if (input.forward) camera_.ProcessKeyboard(FORWARD, dt);
        if (input.backward) camera_.ProcessKeyboard(BACKWARD, dt);
        if (input.left) camera_.ProcessKeyboard(LEFT, dt);
        if (input.right) camera_.ProcessKeyboard(RIGHT, dt);
        camera_.ProcessMouseMovement(input.mouseX, input.mouseY);
        camera_.ProcessMouseScroll(input.scroll);
    }
protected:
    Camera camera_{glm::vec3(0,0,3)};
    glm::mat4 projection(const SceneFrame& frame) {
        return glm::perspective(glm::radians(camera_.Zoom), frame.aspect, .1f, 100.f);
    }
};

class TransformScene final : public CameraScene {
public:
    explicit TransformScene(const std::string& root)
        : shader_(loadShader(root, "intro/transformations")),
          mesh_(cubeVertices(false), {3,2}),
          container_(root + "/resources/textures/container.jpg"),
          face_(root + "/resources/textures/awesomeface.png") {
        shader_->use();
        shader_->setInt("texture0", 0);
        shader_->setInt("texture1", 1);
    }
    void render(const SceneFrame& frame) override {
        glEnable(GL_DEPTH_TEST);
        shader_->use();
        shader_->setMat4("projection", projection(frame));
        shader_->setMat4("view", camera_.GetViewMatrix());
        container_.bind(0);
        face_.bind(1);
        const glm::vec3 positions[] = {
            {0,0,0}, {2,5,-15}, {-1.5f,-2.2f,-2.5f}, {-3.8f,-2,-12.3f},
            {2.4f,-.4f,-3.5f}, {-1.7f,3,-7.5f}, {1.3f,-2,-2.5f},
            {1.5f,2,-2.5f}, {1.5f,.2f,-1.5f}, {-1.3f,1,-1.5f}
        };
        // 一个位置按平移、旋转顺序变换，并绘制十个不同位置的立方体。
        for (int i = 0; i < 10; ++i) {
            auto model = glm::translate(glm::mat4(1), positions[i]);
            model = glm::rotate(model, frame.time * glm::radians(i == 0 ? 50.f : 20.f * i), glm::vec3(.5f,1,0));
            shader_->setMat4("model", model);
            mesh_.draw();
        }
    }
    void drawControls() override {
        // 3D 变换场景同样复用逐纹理采样面板，无需维护另一套环绕/过滤逻辑。
        container_.drawControls("Container (texture0)");
        face_.drawControls("Awesomeface (texture1)");
    }
private:
    std::unique_ptr<Shader> shader_;
    Mesh mesh_;
    Texture container_, face_;
};

// Phong 光照与材质合并为一个可交互场景，避免固定参数的重复光照演示。
class LightingScene final : public CameraScene {
public:
    explicit LightingScene(const std::string& root)
        : shader_(loadShader(root, "lighting/phong-materials", "object")),
          lightShader_(loadShader(root, "lighting/light-source", "light_cube")),
          mesh_(cubeVertices(true), {3,3}) {}
    void render(const SceneFrame& frame) override {
        glEnable(GL_DEPTH_TEST);
        // 光源使用独立动画时间；暂停时仍记录帧时间，恢复后不会补跑暂停期间的动画。
        if (previousFrameTime_ < 0.0f) lightTime_ = frame.time;
        else if (!lightPaused_) lightTime_ += frame.time - previousFrameTime_;
        previousFrameTime_ = frame.time;
        const float angle = lightTime_ * .8f; // 光源每秒旋转 0.8 弧度。
        const glm::vec3 lightPos(std::sin(angle) * lightRadius_, lightHeight_, std::cos(angle) * lightRadius_);
        // 手动颜色只在暂停期间覆盖显示，不修改动画时间，恢复后继续原来的颜色轨迹。
        const glm::vec3 lightColor = lightPaused_ ? pausedLightColor_ : animatedLightColor();
        const auto proj = projection(frame), view = camera_.GetViewMatrix();
        shader_->use();
        shader_->setMat4("projection", proj);
        shader_->setMat4("view", view);
        shader_->setMat4("model", glm::mat4(1));
        shader_->setVec3("viewPos", camera_.Position);
        shader_->setVec3("light.position", lightPos);
        shader_->setVec3("light.ambient", lightColor * .1f);
        shader_->setVec3("light.diffuse", lightColor * .5f);
        shader_->setVec3("light.specular", lightColor);
        shader_->setVec3("material.ambient", materialAmbient_);
        shader_->setVec3("material.diffuse", materialDiffuse_);
        shader_->setVec3("material.specular", materialSpecular_);
        shader_->setFloat("material.shininess", materialShininess_);
        // 先绘制受光物体，再切换灯泡 shader 用同一网格绘制缩小后的光源。
        mesh_.draw();
        lightShader_->use();
        lightShader_->setMat4("projection", proj);
        lightShader_->setMat4("view", view);
        lightShader_->setVec3("lightColor", lightColor);
        auto model = glm::translate(glm::mat4(1), lightPos);
        lightShader_->setMat4("model", glm::scale(model, glm::vec3(.2f)));
        mesh_.draw();
    }
    void drawControls() override {
        // 刚进入暂停时继承当前动画颜色，避免勾选瞬间发生跳色。
        if (ImGui::Checkbox("Pause light animation", &lightPaused_) && lightPaused_)
            pausedLightColor_ = animatedLightColor();
        if (lightPaused_)
            ImGui::ColorEdit3("Paused light color", &pausedLightColor_.x);
        // 参数直接参与光源位置计算；即便动画暂停，改变轨道也会立即更新灯泡和照明位置。
        ImGui::SliderFloat("Light radius", &lightRadius_, 0.0f, 10.0f, "%.2f");
        ImGui::SliderFloat("Light height", &lightHeight_, -5.0f, 5.0f, "%.2f");
        ImGui::ColorEdit3("Material ambient", &materialAmbient_.x);
        ImGui::ColorEdit3("Material diffuse", &materialDiffuse_.x);
        ImGui::ColorEdit3("Material specular", &materialSpecular_.x);
        ImGui::SliderFloat("Material shininess", &materialShininess_, 1.0f, 256.0f, "%.1f");
    }
private:
    glm::vec3 animatedLightColor() const {
        // 正弦波先映射到合法 RGB [0,1]，避免负光强抵消物体颜色。
        return glm::vec3(.5f) + .5f * glm::vec3(
            std::sin(lightTime_ * 2.f), std::sin(lightTime_ * .7f), std::sin(lightTime_ * 1.3f));
    }
    bool lightPaused_ = false;
    float lightTime_ = 0.0f;
    float previousFrameTime_ = -1.0f;
    float lightRadius_ = 2.0f;
    float lightHeight_ = 1.0f;
    glm::vec3 pausedLightColor_{1.0f};
    glm::vec3 materialAmbient_{1.0f, 0.5f, 0.31f};
    glm::vec3 materialDiffuse_{1.0f, 0.5f, 0.31f};
    glm::vec3 materialSpecular_{0.5f, 0.5f, 0.5f};
    float materialShininess_ = 32.0f;
    std::unique_ptr<Shader> shader_, lightShader_;
    Mesh mesh_;
};
}

const std::vector<SceneInfo>& availableScenes() { return sceneInfos; }
// 工厂按面板索引延迟创建单个场景；旧场景由 unique_ptr 自动析构释放资源。
std::unique_ptr<Scene> createScene(int index, const std::string& root) {
    // 索引 0 为清屏场景；其余索引对应列表中的五个教程示例。
    switch (index) {
        case 0: return std::make_unique<EmptyScene>();
        case 1: return std::make_unique<TriangleScene>(root, false);
        case 2: return std::make_unique<TriangleScene>(root, true);
        case 3: return std::make_unique<TransformScene>(root);
        case 4: return std::make_unique<TextureScene>(root);
        case 5: return std::make_unique<LightingScene>(root);
        default: throw std::out_of_range("Invalid scene index");
    }
}
