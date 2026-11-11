#include "ModelScene.h"
#include "Camera.h"
#include "FileDialog.h"
#include "Shader.h"
#include "imgui.h"
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
// 图片由 stb_image 解码，GL 对象由场景独占；构造中途失败也不会遗留半个模型。
class ModelTexture {
public:
    explicit ModelTexture(const unsigned char* rgba, int width, int height) {
        // 解码后的 CPU 像素在 glTexImage2D 中复制到 GPU，上传后不必持续保存原图片内存。
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    ~ModelTexture() { glDeleteTextures(1, &id_); }
    ModelTexture(const ModelTexture&) = delete;
    ModelTexture& operator=(const ModelTexture&) = delete;
    GLuint id() const { return id_; }
private:
    GLuint id_ = 0;
};

struct ModelVertex {
    // position/normal/uv 与 model.vs 的 location 0/1/2 一致，使用观察与光照计算各自所需数据。
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class ModelMesh {
public:
    ModelMesh(const std::vector<ModelVertex>& vertices, const std::vector<unsigned int>& indices,
              glm::vec3 color, std::shared_ptr<ModelTexture> texture)
        : color_(color), texture_(std::move(texture)), count_(static_cast<GLsizei>(indices.size())) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ModelVertex), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        // 使用 offsetof 而不是硬编码 float 步长，以免 GLM 对齐选项改变结构体布局。
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, position)));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, normal)));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, uv)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    ~ModelMesh() {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        glDeleteBuffers(1, &ebo_);
    }
    ModelMesh(const ModelMesh&) = delete;
    ModelMesh& operator=(const ModelMesh&) = delete;
    void draw(Shader& shader) const {
        // 每个子网格可以使用不同材质；必须在其 draw call 之前更新 uniform 和纹理绑定。
        shader.setVec3("baseColor", color_);
        shader.setInt("hasTexture", texture_ ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_ ? texture_->id() : 0);
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    }
private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    glm::vec3 color_;
    std::shared_ptr<ModelTexture> texture_;
    GLsizei count_;
};

class Model {
public:
    explicit Model(const std::string& path) {
        Assimp::Importer importer;
        // 烘焙节点层级变换（包含法线），避免多节点模型的位置、旋转或缩放丢失。
        // 本预览显示静态姿态，不包含骨骼动画播放。
        // Triangulate 把多边形拆成三角形，GenSmoothNormals 为缺失法线的模型补法线；
        // 这些步骤不等于增加模型细节，轮廓精细度仍取决于模型原有的几何数据。
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
            aiProcess_PreTransformVertices | aiProcess_SortByPType | aiProcess_FlipUVs | aiProcess_ValidateDataStructure);
        if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
            throw std::runtime_error(std::string("Model import failed: ") + importer.GetErrorString());

        glm::vec3 lower(std::numeric_limits<float>::max()), upper(std::numeric_limits<float>::lowest());
        const unsigned char white[] = {255, 255, 255, 255};
        const auto fallback = std::make_shared<ModelTexture>(white, 1, 1);
        // 多个材质可以引用同一图片，缓存共享 GPU 纹理，避免重复解码和上传。
        std::map<std::string, std::shared_ptr<ModelTexture>> textureCache;
        const auto directory = std::filesystem::u8path(path).parent_path();
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            const aiMesh& mesh = *scene->mMeshes[i];
            std::vector<unsigned int> indices;
            for (unsigned int f = 0; f < mesh.mNumFaces; ++f) {
                const aiFace& face = mesh.mFaces[f];
                if (face.mNumIndices != 3) continue; // 线、点不能作为 GL_TRIANGLES 绘制。
                indices.insert(indices.end(), face.mIndices, face.mIndices + 3);
            }
            if (indices.empty()) continue;
            if (indices.size() > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
                throw std::runtime_error("Model mesh exceeds OpenGL draw count limit");
            std::vector<ModelVertex> vertices;
            vertices.reserve(mesh.mNumVertices);
            for (unsigned int v = 0; v < mesh.mNumVertices; ++v) {
                const auto& p = mesh.mVertices[v];
                if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                    throw std::runtime_error("Model contains non-finite vertices");
                glm::vec3 normal(0, 1, 0);
                if (mesh.HasNormals()) normal = {mesh.mNormals[v].x, mesh.mNormals[v].y, mesh.mNormals[v].z};
                glm::vec2 uv(0);
                if (mesh.HasTextureCoords(0)) uv = {mesh.mTextureCoords[0][v].x, mesh.mTextureCoords[0][v].y};
                const glm::vec3 position(p.x, p.y, p.z);
                lower = glm::min(lower, position);
                upper = glm::max(upper, position);
                vertices.push_back({position, normal, uv});
            }
            glm::vec3 color(.8f);
            std::shared_ptr<ModelTexture> texture;
            if (mesh.mMaterialIndex < scene->mNumMaterials) {
                const aiMaterial& material = *scene->mMaterials[mesh.mMaterialIndex];
                aiColor4D base;
                if (aiGetMaterialColor(&material, AI_MATKEY_BASE_COLOR, &base) == AI_SUCCESS ||
                    aiGetMaterialColor(&material, AI_MATKEY_COLOR_DIFFUSE, &base) == AI_SUCCESS)
                    color = {base.r, base.g, base.b};
                // 只把基础颜色/漫反射图当颜色采样；Bump/Normal/金属度图不能直接混作颜色图。
                aiString texturePath;
                if (material.GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == AI_SUCCESS ||
                    material.GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
                    const std::string key = texturePath.C_Str();
                    if (textureCache.count(key)) texture = textureCache.at(key);
                    else {
                        try { texture = loadTexture(*scene, key, directory); }
                        catch (const std::exception& error) { warnings_ += std::string(error.what()) + "\n"; }
                        textureCache.emplace(key, texture);
                    }
                }
            }
            // 无贴图时绑定完整的白色纹理，避免驱动检查到不完整的默认纹理对象。
            if (!texture) texture = fallback;
            triangles_ += indices.size() / 3;
            meshes_.push_back(std::make_unique<ModelMesh>(vertices, indices, color, std::move(texture)));
        }
        if (meshes_.empty()) throw std::runtime_error("Model contains no triangle meshes");
        center_ = lower * .5f + upper * .5f;
        // 包围盒中点用来居中，最大边长用来统一显示尺度，不改动源模型文件。
        const glm::vec3 extent = upper - lower;
        const float size = std::max({extent.x, extent.y, extent.z});
        if (!std::isfinite(size)) throw std::runtime_error("Model bounds exceed float range");
        // 归一化到最大边长 2，相机速度/近远裁剪不再依赖模型原始单位。
        scale_ = size > 1e-8f ? 2.f / size : 1.f;
        // importer 即将析构；所有需要的网格和纹理数据此时均已复制到 GPU。
    }
    // 顶点实际经历：移到模型中心 -> 统一缩放 -> X/Y/Z 旋转；矩阵作用顺序由右向左。
    void draw(Shader& shader, float rotationX, float rotationY, float rotationZ) const {
        const glm::mat4 orientation = glm::rotate(glm::mat4(1), glm::radians(rotationZ), glm::vec3(0,0,1)) *
                                     glm::rotate(glm::mat4(1), glm::radians(rotationY), glm::vec3(0,1,0)) *
                                     glm::rotate(glm::mat4(1), glm::radians(rotationX), glm::vec3(1,0,0));
        shader.setMat4("model", orientation * glm::scale(glm::mat4(1), glm::vec3(scale_)) *
                                 glm::translate(glm::mat4(1), -center_));
        for (const auto& mesh : meshes_) mesh->draw(shader);
    }
    size_t meshCount() const { return meshes_.size(); }
    size_t triangles() const { return triangles_; }
    const std::string& warnings() const { return warnings_; }
private:
    static std::shared_ptr<ModelTexture> loadTexture(const aiScene& scene, const std::string& path,
                                                    const std::filesystem::path& directory) {
        std::vector<unsigned char> bytes;
        const aiTexture* embedded = scene.GetEmbeddedTexture(path.c_str());
        if (embedded && embedded->mHeight != 0) {
            // 未压缩的内嵌贴图是 aiTexel BGRA，显式转为 RGBA 后上传。
            std::vector<unsigned char> rgba(static_cast<size_t>(embedded->mWidth) * embedded->mHeight * 4);
            for (size_t i = 0; i < rgba.size() / 4; ++i) {
                rgba[i*4] = embedded->pcData[i].r; rgba[i*4+1] = embedded->pcData[i].g;
                rgba[i*4+2] = embedded->pcData[i].b; rgba[i*4+3] = embedded->pcData[i].a;
            }
            return std::make_shared<ModelTexture>(rgba.data(), embedded->mWidth, embedded->mHeight);
        }
        // mHeight=0 表示压缩图像，mWidth 此时是字节数（不是像素宽度）。
        if (embedded) {
            const auto* data = reinterpret_cast<const unsigned char*>(embedded->pcData);
            bytes.assign(data, data + embedded->mWidth);
        } else {
            std::string portable = path;
            std::replace(portable.begin(), portable.end(), '\\', '/');
            // C++17 filesystem::u8path + ifstream 支持 Windows 中文目录，避免 ANSI fopen。
            const auto resolved = directory / std::filesystem::u8path(portable);
            std::ifstream stream(resolved, std::ios::binary);
            if (!stream) throw std::runtime_error("Missing texture: " + resolved.u8string());
            bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }
        if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::runtime_error("Invalid texture size: " + path);
        // 导入时已翻转 UV；解码保持图像原始行序，UI 还可修正特殊模型的 V 方向。
        stbi_set_flip_vertically_on_load(false);
        int width = 0, height = 0, channels = 0;
        std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
            stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4), stbi_image_free);
        if (!pixels) throw std::runtime_error("Cannot decode texture: " + path);
        return std::make_shared<ModelTexture>(pixels.get(), width, height);
    }
    glm::vec3 center_{0};
    float scale_ = 1;
    size_t triangles_ = 0;
    std::vector<std::unique_ptr<ModelMesh>> meshes_;
    std::string warnings_;
};

class ModelScene final : public Scene {
public:
    ModelScene(const std::string& root, const std::string& path)
        : shader_((root + "/resources/shaders/model/model.vs").c_str(),
                  (root + "/resources/shaders/model/model.fs").c_str()) {
        if (!path.empty()) load(path); // 启动路径失败直接报告给调用者，不留下空白的成功状态。
    }
    bool usesCamera() const override { return true; }
    void input(const SceneInput& in, float dt) override {
        // 模态文件对话框返回时丢弃旧鼠标位移，避免恢复后的第一帧视角突然跳动。
        if (skipInput_) { skipInput_ = false; return; }
        if (in.forward) camera_.ProcessKeyboard(FORWARD, dt);
        if (in.backward) camera_.ProcessKeyboard(BACKWARD, dt);
        if (in.left) camera_.ProcessKeyboard(LEFT, dt);
        if (in.right) camera_.ProcessKeyboard(RIGHT, dt);
        camera_.ProcessMouseMovement(in.mouseX, in.mouseY);
        camera_.ProcessMouseScroll(in.scroll);
    }
    void render(const SceneFrame& frame) override {
        glEnable(GL_DEPTH_TEST);
        if (!model_) return;
        shader_.use();
        shader_.setMat4("view", camera_.GetViewMatrix());
        shader_.setMat4("projection", glm::perspective(glm::radians(camera_.Zoom), frame.aspect, .01f, 100.f));
        shader_.setVec3("viewPos", camera_.Position);
        shader_.setInt("diffuseMap", 0);
        shader_.setInt("flipV", flipV_);
        // UI 是角度的唯一来源，每帧传入模型矩阵，不能再在 Model 中保存另一份角度。
        model_->draw(shader_, rotationX_, rotationY_, rotationZ_);
    }
    void drawControls() override {
        if (ImGui::Button("Open local model...")) {
            skipInput_ = true;
            try {
                const std::string selected = selectModelFile();
                if (!selected.empty()) load(selected);
            } catch (const std::exception& error) { error_ = error.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset view")) camera_ = Camera(glm::vec3(0, 0, 4.5f));
        ImGui::TextWrapped("OBJ / FBX / glTF / GLB / DAE / PLY / STL ...");
        ImGui::TextUnformatted("WASD: move | RMB drag: look | Scroll: zoom");
        if (!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
        if (model_) {
            ImGui::TextWrapped("%s", path_.c_str());
            ImGui::Text("Meshes: %zu | Triangles: %zu", model_->meshCount(), model_->triangles());
            ImGui::Separator();
            ImGui::TextUnformatted("Model orientation (drag sliders below)");
            ImGui::SliderFloat("Rotate X", &rotationX_, -180.0f, 180.0f, "%.1f deg");
            ImGui::SliderFloat("Rotate Y", &rotationY_, -180.0f, 180.0f, "%.1f deg");
            ImGui::SliderFloat("Rotate Z", &rotationZ_, -180.0f, 180.0f, "%.1f deg");
            ImGui::SameLine();
            if (ImGui::Button("Reset orientation")) resetOrientation();
            ImGui::Checkbox("Flip texture V", &flipV_);
            if (!model_->warnings().empty()) ImGui::TextWrapped("%s", model_->warnings().c_str());
        } else ImGui::TextWrapped("Select a model to preview. The model will be centered and scaled automatically.");
    }
private:
    void resetOrientation() {
        rotationX_ = rotationY_ = rotationZ_ = 0;
        // 此模型说明为 Z-up、正面 +Y。Rx(-90°) 将 +Z 转成 +Y，
        // 再 Ry(180°) 将原来的正面转向相机所在的 +Z，且保持头顶朝上。
        // OBJ 不携带统一的朝向语义，只为这个已知模型提供初始预设。
        if (std::filesystem::u8path(path_).filename().u8string() == "capylulu_lulu.obj") {
            rotationX_ = -90.0f;
            rotationY_ = 180.0f;
        }
    }
    void load(const std::string& path) {
        auto next = std::make_unique<Model>(path);
        // 成功后再替换，失败时旧模型仍可见、可操作。
        model_ = std::move(next);
        path_ = path;
        resetOrientation();
        error_.clear();
        flipV_ = false;
        camera_ = Camera(glm::vec3(0, 0, 4.5f));
    }
    Shader shader_;
    Camera camera_{glm::vec3(0, 0, 4.5f)};
    std::unique_ptr<Model> model_;
    std::string path_, error_;
    bool flipV_ = false;
    bool skipInput_ = false;
    float rotationX_ = 0, rotationY_ = 0, rotationZ_ = 0;
};
}

std::unique_ptr<Scene> createModelScene(const std::string& root, const std::string& modelPath) {
    return std::make_unique<ModelScene>(root, modelPath);
}
