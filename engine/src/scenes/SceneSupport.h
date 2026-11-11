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
// Mesh（网格）表示可绘制的几何体，封装下面三个 OpenGL 对象：
//
// VBO（Vertex Buffer Object，顶点缓冲对象）：保存顶点数据的缓冲区。
//   CPU 的 vertices 数组不能直接供 GPU 持续读取，因此要把位置、颜色、UV 等数据
//   上传到 OpenGL 管理的缓冲区。VBO 只存数据，不知道哪些 float 是位置、哪些是 UV。
//
// VAO（Vertex Array Object，顶点数组对象）：记录“如何读取顶点数据”的配置。
//   例如：属性 0 从哪个 VBO 读取、每次读几个分量、顶点之间隔多少字节、从哪里开始。
//   它还记录属性是否启用以及 EBO 的绑定，但不保存顶点数据本身，也不保存 shader/纹理。
//   可以把 VBO 理解为“原始数据仓库”，VAO 理解为“读取这份数据的说明书”。
//
// EBO（Element Buffer Object，也称索引缓冲对象）：保存顶点的编号，而非顶点属性。
//   例如用 4 个顶点画矩形，索引 {0,1,2, 0,2,3} 表示两个三角形，复用顶点 0 和 2，
//   不必为这两个三角形重复存储 6 份顶点。没有索引时可以不创建 EBO。
//
// 本类在构造时上传一次数据并配置 VAO；每帧绑定 VAO 后提交绘制，不必重复上传或配置。
// 注意：绑定 VAO 并不会自动绘制，仍须调用 glDrawArrays 或 glDrawElements。
class Mesh {
public:
    Mesh(const std::vector<float>& vertices, std::initializer_list<int> attributes,
         const std::vector<unsigned int>& indices = {}) {
        // attributes={3,2} 表示每顶点是 vec3 位置 + vec2 UV，总步长为 5 个 float。
        // 属性的顺序必须与 GLSL 中 layout(location=0/1/...) 的声明一一对应。
        int stride = 0;
        for (int size : attributes) stride += size;
        // glGen* 取得对象名称（整数 ID），不是 CPU 内存地址；此时尚未上传顶点内容。
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        // 先选中要配置的 VAO，后面的属性布局和 EBO 绑定才会记录到这个 VAO 中。
        glBindVertexArray(vao_);
        // GL_ARRAY_BUFFER 是操作顶点缓冲的绑定目标；bind 只是选中对象，不会复制数据。
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        // glBufferData 会把数据复制到 GPU；STATIC_DRAW 表示内容预计很少修改。
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        if (!indices.empty()) {
            // 索引按 unsigned int 上传；绘制时 GL_UNSIGNED_INT 必须与这里的数据类型一致。
            glGenBuffers(1, &ebo_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        }
        // offset 是属性在单个顶点内的位置（以 float 为单位）；API 接收字节偏移。
        // glVertexAttribPointer 在 VAO 中记录当前 VBO，无需每帧重新描述布局。
        int offset = 0;
        unsigned int location = 0;
        for (int size : attributes) {
            // 参数依次是：shader 属性编号、分量数、类型、是否归一化、顶点步长、属性字节偏移。
            // 最后一个参数在绑定 VBO 时表示缓冲区内的偏移，不是指向 CPU 顶点数组的指针。
            // 例如 {3,2}：位置从偏移 0 读 3 个 float，UV 从偏移 3*sizeof(float) 读 2 个。
            glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                                  reinterpret_cast<void*>(static_cast<size_t>(offset) * sizeof(float)));
            // 描述布局和启用属性是两件事；不启用时 shader 不会从该顶点数组读取此属性。
            glEnableVertexAttribArray(location++);
            offset += size;
        }
        count_ = static_cast<GLsizei>(indices.empty() ? vertices.size() / stride : indices.size());
        // EBO 绑定属于 VAO 状态，因此不要在 VAO 仍绑定时把 EBO 解绑为 0。
        glBindVertexArray(0);
        // GL_ARRAY_BUFFER 的当前绑定本身不属于 VAO；属性已记住 VBO，解绑不会丢失关联。
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    ~Mesh() {
        // 析构时 GL 上下文仍须有效；禁用拷贝防止两个对象重复删除同一组 ID。
        glDeleteBuffers(1, &ebo_);
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
    }
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    void draw() const {
        // 恢复此前记录的属性布局、VBO 关联和 EBO 绑定；shader、纹理仍由场景自行设置。
        glBindVertexArray(vao_);
        // 有 EBO 时 count_ 是索引数；否则是顶点数，均不是三角形个数。
        // DrawElements 从 EBO 按索引取顶点，nullptr 表示从索引缓冲的字节偏移 0 开始；
        // DrawArrays 则从顶点编号 0 开始顺序读取。GL_TRIANGLES 每 3 个顶点组成一个三角形。
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
        // 常见图片以左上角为起点；本教程的二维 UV 以左下角为起点，所以翻转图像行。
        // 统一解码为 RGBA，保留 PNG 透明度，也避免 RGB 行宽带来的默认 4 字节对齐问题。
        stbi_set_flip_vertically_on_load(true);
        unsigned char* pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels) throw std::runtime_error("Cannot load texture: " + filename);
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        // GL_RGBA8 是 GPU 存储格式；GL_RGBA/UNSIGNED_BYTE 描述 CPU 像素格式和元素类型。
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
        // 生成逐级缩小至 1x1 的 mipmap，供缩小过滤选择；不是每帧重建。
        glGenerateMipmap(GL_TEXTURE_2D);
        applySampling();
    }
    ~Texture() { glDeleteTextures(1, &id_); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    void bind(unsigned int unit) const {
        // 纹理单元是采样器的槽位，不是纹理 ID。shader.setInt("sampler", unit) 传槽位编号。
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
        // 键盘移动乘帧间隔以保持速度不随 FPS 变化；鼠标输入已经是本帧累计位移。
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
        // GLM 角度用弧度；aspect 由帧缓冲计算，以适配拖动窗口和高分屏。
        // .1/100 是近远裁剪面，近平面不能为 0，过小也会损失远处的深度精度。
        return glm::perspective(glm::radians(camera_.Zoom), frame.aspect, .1f, 100.f);
    }
};


std::unique_ptr<Shader> loadShader(const std::string& root, const std::string& directory,
                                   const std::string& name = "shader");
std::vector<float> cubeVertices(bool withNormals);
} // namespace sceneSupport
