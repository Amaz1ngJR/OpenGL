#include "ModelScene.h"
#include "Camera.h"
#include "FileDialog.h"
#include "Shader.h"
#include "scenes/SceneModel.h"
#include "imgui.h"
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_inverse.hpp>
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
    void update(const std::vector<ModelVertex>& vertices) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size()*sizeof(ModelVertex), vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    ~ModelMesh() {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        glDeleteBuffers(1, &ebo_);
    }
    ModelMesh(const ModelMesh&) = delete;
    ModelMesh& operator=(const ModelMesh&) = delete;
    void draw(Shader& shader, bool materials = true) const {
        // 每个子网格可以使用不同材质；必须在其 draw call 之前更新 uniform 和纹理绑定。
        if (materials) {
            shader.setVec3("baseColor", color_);
            shader.setInt("hasTexture", texture_ ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture_ ? texture_->id() : 0);
        }
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    }
private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    glm::vec3 color_;
    std::shared_ptr<ModelTexture> texture_;
    GLsizei count_;
};

class Model final : public SceneModel {
public:
    explicit Model(const std::string& path, bool animated) : animated_(animated) {
        // 烘焙节点层级变换（包含法线），避免多节点模型的位置、旋转或缩放丢失。
        // 动画模式保留节点层级；静态模式烘焙变换。
        // Triangulate 把多边形拆成三角形，GenSmoothNormals 为缺失法线的模型补法线；
        // 这些步骤不等于增加模型细节，轮廓精细度仍取决于模型原有的几何数据。
        const aiScene* scene = importer_.ReadFile(path, aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals |
            (animated ? 0 : aiProcess_PreTransformVertices) | aiProcess_SortByPType | aiProcess_FlipUVs | aiProcess_ValidateDataStructure);
        if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
            throw std::runtime_error(std::string("Model import failed: ") + importer_.GetErrorString());

        scene_ = scene;
        if(animated_) for(unsigned i=0;i<scene->mNumAnimations;++i)
            if(scene->mAnimations[i]->mNumMorphMeshChannels || scene->mAnimations[i]->mNumMeshChannels)
                warnings_ += "Morph / vertex animation channels are not supported; only node/bone animation is played.\n";
        std::vector<std::pair<unsigned, const aiNode*>> entries;
        if (animated_) {
            collect(scene->mRootNode, glm::mat4(1), entries);
        } else for(unsigned i=0;i<scene->mNumMeshes;++i) entries.push_back({i,nullptr});
        glm::vec3 lower(std::numeric_limits<float>::max()), upper(std::numeric_limits<float>::lowest());
        const unsigned char white[] = {255, 255, 255, 255};
        const auto fallback = std::make_shared<ModelTexture>(white, 1, 1);
        // 多个材质可以引用同一图片，缓存共享 GPU 纹理，避免重复解码和上传。
        std::map<std::string, std::shared_ptr<ModelTexture>> textureCache;
        const auto directory = std::filesystem::u8path(path).parent_path();
        for (const auto& entry : entries) {
            const aiMesh& mesh = *scene->mMeshes[entry.first];
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
                const glm::vec3 boundPosition = entry.second ? glm::vec3(globals_.at(entry.second->mName.C_Str()) * glm::vec4(position,1)) : position;
                lower = glm::min(lower, boundPosition);
                upper = glm::max(upper, boundPosition);
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
            if(animated_) sources_.push_back({vertices,&mesh,entry.second});
            triangles_ += indices.size() / 3;
            meshes_.push_back(std::make_unique<ModelMesh>(vertices, indices, color, std::move(texture)));
        }
        if (meshes_.empty()) throw std::runtime_error("Model contains no triangle meshes");
        center_ = lower * .5f + upper * .5f;
        groundY_ = lower.y;
        // 包围盒中点用来居中，最大边长用来统一显示尺度，不改动源模型文件。
        const glm::vec3 extent = upper - lower;
        const float size = std::max({extent.x, extent.y, extent.z});
        if (!std::isfinite(size)) throw std::runtime_error("Model bounds exceed float range");
        // 归一化到最大边长 2，相机速度/近远裁剪不再依赖模型原始单位。
        scale_ = size > 1e-8f ? 2.f / size : 1.f;
        if(animated_) update(-1,0);
        // importer 保留节点、动画和权重，生命周期覆盖所有播放帧。
    }
    unsigned animationCount() const { return animated_ ? scene_->mNumAnimations : 0; }
    const char* animationName(unsigned clip) const { return scene_->mAnimations[clip]->mName.C_Str(); }
    double duration(unsigned clip) const override {
        if (clip >= animationCount()) return 0;
        const auto& a=*scene_->mAnimations[clip];
        return a.mDuration / (a.mTicksPerSecond>0 ? a.mTicksPerSecond : 25.0);
    }
    void update(int clip, double seconds) override {
        if(!animated_) return;
        const aiAnimation* animation=clip>=0 && unsigned(clip)<animationCount() ? scene_->mAnimations[clip] : nullptr;
        double ticks=animation ? seconds*(animation->mTicksPerSecond>0 ? animation->mTicksPerSecond : 25.0) : 0;
        channels_.clear();
        if(animation) for(unsigned i=0;i<animation->mNumChannels;++i)
            channels_[animation->mChannels[i]->mNodeName.C_Str()]=animation->mChannels[i];
        evaluate(scene_->mRootNode,glm::mat4(1),ticks);
        for(size_t i=0;i<sources_.size();++i) {
            const auto& source=sources_[i];
            auto vertices=source.vertices;
            std::vector<glm::mat4> skin(vertices.size(),glm::mat4(0));
            std::vector<float> totals(vertices.size(),0);
            for(unsigned b=0;b<source.mesh->mNumBones;++b) {
                const auto& bone=*source.mesh->mBones[b];
                auto found=globals_.find(bone.mName.C_Str());
                if(found==globals_.end()) throw std::runtime_error("Bone node missing: "+std::string(bone.mName.C_Str()));
                glm::mat4 transform=found->second*matrix(bone.mOffsetMatrix);
                for(unsigned w=0;w<bone.mNumWeights;++w) {
                    const auto& weight=bone.mWeights[w];
                    if(weight.mVertexId>=vertices.size() || weight.mWeight<=0) continue;
                    skin[weight.mVertexId]+=transform*weight.mWeight;
                    totals[weight.mVertexId]+=weight.mWeight;
                }
            }
            for(size_t v=0;v<vertices.size();++v) {
                glm::mat4 transform=totals[v]>0 ? skin[v]/totals[v] : globals_.at(source.node->mName.C_Str());
                vertices[v].position=glm::vec3(transform*glm::vec4(source.vertices[v].position,1));
                glm::mat3 basis(transform);
                if(std::abs(glm::determinant(basis))>1e-10f)
                    vertices[v].normal=glm::normalize(glm::inverseTranspose(basis)*source.vertices[v].normal);
            }
            meshes_[i]->update(vertices);
        }
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
    void drawPlaced(Shader& shader, const glm::mat4& placement, bool materials) const override {
        const glm::vec3 anchor(center_.x, groundY_, center_.z);
        shader.setMat4("model", placement * glm::scale(glm::mat4(1), glm::vec3(scale_)) *
                                 glm::translate(glm::mat4(1), -anchor));
        for (const auto& mesh : meshes_) mesh->draw(shader, materials);
    }
    size_t meshCount() const { return meshes_.size(); }
    size_t triangles() const { return triangles_; }
    const std::string& warnings() const { return warnings_; }
private:
    static glm::mat4 matrix(const aiMatrix4x4& m) {
        return glm::mat4(m.a1,m.b1,m.c1,m.d1, m.a2,m.b2,m.c2,m.d2,
                         m.a3,m.b3,m.c3,m.d3, m.a4,m.b4,m.c4,m.d4);
    }
    void collect(const aiNode* node, const glm::mat4& parent,
                 std::vector<std::pair<unsigned,const aiNode*>>& entries) {
        glm::mat4 global=parent*matrix(node->mTransformation);
        globals_[node->mName.C_Str()]=global;
        for(unsigned i=0;i<node->mNumMeshes;++i) entries.push_back({node->mMeshes[i],node});
        for(unsigned i=0;i<node->mNumChildren;++i) collect(node->mChildren[i],global,entries);
    }
    template<class Key, class Value, class Blend>
    static Value sample(const Key* keys,unsigned count,double time,Value fallback,Blend blend) {
        if(!count) return fallback;
        if(count==1 || time<=keys[0].mTime) return keys[0].mValue;
        for(unsigned i=1;i<count;++i) if(time<keys[i].mTime) {
            double span=keys[i].mTime-keys[i-1].mTime;
            return blend(keys[i-1].mValue,keys[i].mValue,float(span>0 ? (time-keys[i-1].mTime)/span : 0));
        }
        return keys[count-1].mValue;
    }
    void evaluate(const aiNode* node,const glm::mat4& parent,double time) {
        aiMatrix4x4 local=node->mTransformation;
        auto channel=channels_.find(node->mName.C_Str());
        if(channel!=channels_.end()) {
            const auto& c=*channel->second;
            aiVector3D scale,position; aiQuaternion rotation;
            local.Decompose(scale,rotation,position);
            auto lerp=[](aiVector3D a,aiVector3D b,float t){return a+(b-a)*t;};
            position=sample(c.mPositionKeys,c.mNumPositionKeys,time,position,lerp);
            scale=sample(c.mScalingKeys,c.mNumScalingKeys,time,scale,lerp);
            rotation=sample(c.mRotationKeys,c.mNumRotationKeys,time,rotation,
                [](aiQuaternion a,aiQuaternion b,float t){aiQuaternion r; aiQuaternion::Interpolate(r,a,b,t); return r.Normalize();});
            local=aiMatrix4x4(scale,rotation,position);
        }
        glm::mat4 global=parent*matrix(local);
        globals_[node->mName.C_Str()]=global;
        for(unsigned i=0;i<node->mNumChildren;++i) evaluate(node->mChildren[i],global,time);
    }
    struct Source { std::vector<ModelVertex> vertices; const aiMesh* mesh; const aiNode* node; };
    Assimp::Importer importer_;
    const aiScene* scene_=nullptr;
    bool animated_=false;
    std::vector<Source> sources_;
    std::map<std::string,glm::mat4> globals_;
    std::map<std::string,const aiNodeAnim*> channels_;
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
    float groundY_ = 0;
    float scale_ = 1;
    size_t triangles_ = 0;
    std::vector<std::unique_ptr<ModelMesh>> meshes_;
    std::string warnings_;
};

// Polygon/offset/color-mask state must not leak into ImGui or other scenes.
struct ModelRenderState {
    GLint polygon[2],function;
    GLfloat factor,units;
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),
              offset=glIsEnabled(GL_POLYGON_OFFSET_FILL),mask,color[4];
    ModelRenderState() {
        glGetIntegerv(GL_POLYGON_MODE,polygon); glGetIntegerv(GL_DEPTH_FUNC,&function);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&factor); glGetFloatv(GL_POLYGON_OFFSET_UNITS,&units);
        glGetBooleanv(GL_DEPTH_WRITEMASK,&mask); glGetBooleanv(GL_COLOR_WRITEMASK,color);
    }
    ~ModelRenderState() {
        glPolygonMode(GL_FRONT_AND_BACK,polygon[0]); glPolygonOffset(factor,units);
        glDepthFunc(function); glDepthMask(mask); glColorMask(color[0],color[1],color[2],color[3]);
        restore(GL_DEPTH_TEST,depth); restore(GL_BLEND,blend); restore(GL_CULL_FACE,cull); restore(GL_POLYGON_OFFSET_FILL,offset);
    }
    static void restore(GLenum cap,GLboolean enabled) { if(enabled) glEnable(cap); else glDisable(cap); }
};

class ModelScene final : public Scene {
public:
    ModelScene(const std::string& root, const std::string& path, const ModelViewOptions& options)
        : shader_((root + "/resources/shaders/model/model.vs").c_str(),
                  (root + "/resources/shaders/model/model.fs").c_str()) {
        loadAnimations_=options.skeletalAnimation;
        showTextures_=options.textures; flatShading_=options.flatShading;
        displayMode_=static_cast<int>(options.display);
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
        if (!model_) return;
        if(model_->animationCount()) {
            double length=model_->duration(clip_);
            if(playing_ && !bindPose_) {
                animationTime_+=frame.deltaTime*playbackSpeed_;
                if(length>0 && animationTime_>=length) {
                    if(loop_) animationTime_=std::fmod(animationTime_,length);
                    else { animationTime_=length; playing_=false; }
                }
            }
            model_->update(bindPose_ ? -1 : clip_,animationTime_);
        }
        ModelRenderState previous;
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        shader_.use();
        shader_.setMat4("view", camera_.GetViewMatrix());
        shader_.setMat4("projection", glm::perspective(glm::radians(camera_.Zoom), frame.aspect, .01f, 100.f));
        shader_.setVec3("viewPos", camera_.Position);
        shader_.setInt("diffuseMap", 0);
        shader_.setInt("flipV", flipV_);
        shader_.setInt("showTextures",showTextures_);
        shader_.setInt("flatShading",flatShading_);
        shader_.setInt("wirePass",false);
        shader_.setVec3("surfaceColor",surfaceColor_);
        shader_.setVec3("wireColor",displayMode_==2 ? glm::vec3(.85f) : glm::vec3(.06f));
        // Push the filled surface back slightly so coplanar triangle edges do not flicker.
        if(displayMode_!=0) { glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(1,1); }
        else glDisable(GL_POLYGON_OFFSET_FILL);
        if(displayMode_==2) glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
        model_->draw(shader_, rotationX_, rotationY_, rotationZ_);
        if(displayMode_!=0) {
            glColorMask(previous.color[0],previous.color[1],previous.color[2],previous.color[3]);
            glDisable(GL_POLYGON_OFFSET_FILL);
            glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
            shader_.setInt("wirePass",true);
            model_->draw(shader_, rotationX_, rotationY_, rotationZ_);
        }
    }
    void drawControls() override {
        if (ImGui::Checkbox("Load skeletal animations", &loadAnimations_) && model_) {
            try {
                // 先完整创建替代模型；失败时保留旧模型与原加载模式。
                auto next = std::make_unique<Model>(path_, loadAnimations_);
                glUseProgram(0); glBindVertexArray(0);
                model_ = std::move(next);
                loadedAnimations_ = loadAnimations_;
                clip_ = 0; animationTime_ = 0; playing_ = true; bindPose_ = false;
                error_.clear();
            } catch (const std::exception& error) {
                loadAnimations_ = loadedAnimations_;
                error_ = error.what();
            }
        }
        ImGui::TextWrapped("Enable to play skeletal animations in the current model (FBX / glTF / GLB / DAE). Toggling reloads the model and preserves your view.");
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
            if(model_->animationCount()) {
                ImGui::SeparatorText("Animation playback");
                if(ImGui::BeginCombo("Animation clip",model_->animationName(clip_))) {
                    for(unsigned i=0;i<model_->animationCount();++i) {
                        std::string label=std::to_string(i+1)+": "+model_->animationName(i);
                        if(ImGui::Selectable(label.c_str(),clip_==int(i))) {clip_=i; animationTime_=0;}
                    }
                    ImGui::EndCombo();
                }
                ImGui::Checkbox("Play",&playing_); ImGui::SameLine(); ImGui::Checkbox("Loop",&loop_);
                ImGui::Checkbox("Bind pose",&bindPose_);
                ImGui::SliderFloat("Playback speed",&playbackSpeed_,.1f,3.f);
                float time=float(animationTime_);
                if(ImGui::SliderFloat("Time (seconds)",&time,0,float(model_->duration(clip_)))) animationTime_=time;
                if(ImGui::Button("Restart animation")) {animationTime_=0; playing_=true; bindPose_=false;}
            } else if(loadedAnimations_) ImGui::TextWrapped("No animation clips in this file; showing bind pose.");
            ImGui::Separator();
            ImGui::TextUnformatted("Model orientation (drag sliders below)");
            ImGui::SliderFloat("Rotate X", &rotationX_, -180.0f, 180.0f, "%.1f deg");
            ImGui::SliderFloat("Rotate Y", &rotationY_, -180.0f, 180.0f, "%.1f deg");
            ImGui::SliderFloat("Rotate Z", &rotationZ_, -180.0f, 180.0f, "%.1f deg");
            ImGui::SameLine();
            if (ImGui::Button("Reset orientation")) resetOrientation();
            ImGui::SeparatorText("Surface and topology");
            ImGui::Checkbox("Show textures",&showTextures_);
            const char* modes[]={"Surface", "Surface + triangle edges", "Triangle wireframe"};
            ImGui::Combo("Display mode",&displayMode_,modes,3);
            ImGui::Checkbox("Flat triangle shading",&flatShading_);
            if(!showTextures_) ImGui::ColorEdit3("Surface color",&surfaceColor_.x);
            ImGui::TextWrapped("Turn textures off to inspect a neutral mesh surface. Flat shading shows each triangle face; triangle edges show the imported triangulation. Hidden edges are depth-tested.");
            if(showTextures_) ImGui::Checkbox("Flip texture V", &flipV_);
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
        auto next = std::make_unique<Model>(path,loadAnimations_);
        // 成功后再替换，失败时旧模型仍可见、可操作。
        model_ = std::move(next);
        loadedAnimations_=loadAnimations_; clip_=0; animationTime_=0; playing_=true; bindPose_=false;
        path_ = path;
        resetOrientation();
        error_.clear();
        flipV_ = false;
        camera_ = Camera(glm::vec3(0, 0, 4.5f));
    }
    bool loadAnimations_=false,loadedAnimations_=false,playing_=true,loop_=true,bindPose_=false;
    int clip_=0;
    double animationTime_=0;
    float playbackSpeed_=1;
    Shader shader_;
    Camera camera_{glm::vec3(0, 0, 4.5f)};
    std::unique_ptr<Model> model_;
    std::string path_, error_;
    bool showTextures_=true,flatShading_=false;
    int displayMode_=0;
    glm::vec3 surfaceColor_{.75f};
    bool flipV_ = false;
    bool skipInput_ = false;
    float rotationX_ = 0, rotationY_ = 0, rotationZ_ = 0;
};
}

std::unique_ptr<Scene> createModelScene(const std::string& root, const std::string& modelPath, const ModelViewOptions& options) {
    return std::make_unique<ModelScene>(root, modelPath, options);
}

std::unique_ptr<SceneModel> loadSceneModel(const std::string& path, bool animated) {
    return std::make_unique<Model>(path, animated);
}
