#pragma once
#include "Scene.h"
#include "Shader.h"
#include "Camera.h"
#include "imgui.h"
#include "stb_image.h"
#include <glm/gtc/matrix_transform.hpp>
#include <initializer_list>
#include <stdexcept>
#include <vector>

// 教程场景共用的内部辅助代码；GPU 资源保持独占所有权。
namespace sceneSupport {
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


class Texture {
public:
    explicit Texture(const std::string& filename, GLenum wrap = GL_REPEAT)
        : wrapS_(wrap == GL_MIRRORED_REPEAT ? 1 : wrap == GL_CLAMP_TO_EDGE ? 2 : wrap == GL_CLAMP_TO_BORDER ? 3 : 0), wrapT_(wrapS_) {
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


std::unique_ptr<Shader> loadShader(const std::string& root, const std::string& directory,
                                   const std::string& name = "shader");
std::vector<float> cubeVertices(bool withNormals);
} // namespace sceneSupport
