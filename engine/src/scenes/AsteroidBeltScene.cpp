#include "SceneFactories.h"
#include "SceneSupport.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>

using namespace sceneSupport;
namespace {
// 背景天空盒的六个面按 GL 约定顺序排列：+X、-X、+Y、-Y、+Z、-Z。
// 素材文件名用的正是这套方向命名（p = 正向，n = 负向），所以顺序即文件名顺序，
// 不需要像常见的 right/left/front/back 那样再做一次人工重映射。
const char* const kSpaceFaces[] = {"px.jpg", "nx.jpg", "py.jpg",
                                   "ny.jpg", "pz.jpg", "nz.jpg"};

// 立方体贴图是一个纹理对象的六个面；采样时传三维方向，而不是二维 UV。
// 与 cubemaps 场景的 Cubemap 相同，但这里读取的是本场景专属的星空素材目录。
class SpaceCubemap {
public:
    explicit SpaceCubemap(const std::string& root) {
        GLint binding = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
        // 天空盒图片遵循自身的朝向约定，不能沿用二维纹理的垂直翻转，否则接缝和上下方向会错位。
        // 该开关是 stb_image 的全局状态，函数返回前必须恢复，避免影响其他场景的贴图解码。
        stbi_set_flip_vertically_on_load(false);
        try {
            int faceSize = 0;
            for (int i = 0; i < 6; ++i) {
                const std::string path = root + "/resources/textures/skybox_space/" + kSpaceFaces[i];
                int w = 0, h = 0, channels = 0;
                unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
                if (!data) throw std::runtime_error("Cannot load space cubemap face: " + path);
                // 六个面必须同为正方形且尺寸一致，否则立方体贴图不完整，采样结果不可预期。
                if (w != h || (i && w != faceSize)) {
                    stbi_image_free(data);
                    throw std::runtime_error("Space cubemap faces must be equal-sized squares: " + path);
                }
                faceSize = w;
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, w, h, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
            }
            // 星星是高频细节：缩小过滤使用三线性 mipmap，避免远处出现闪烁噪点。
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // 三个轴都钳制到边缘，防止跨面边缘采样时出现重复图像。
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        } catch (...) {
            // 任意一面失败都要清理纹理、恢复全局解码开关和 GL 绑定，再继续抛出。
            stbi_set_flip_vertically_on_load(true);
            glBindTexture(GL_TEXTURE_CUBE_MAP, binding);
            glDeleteTextures(1, &id_);
            throw;
        }
        stbi_set_flip_vertically_on_load(true);
        glBindTexture(GL_TEXTURE_CUBE_MAP, binding);
    }
    ~SpaceCubemap() { glDeleteTextures(1, &id_); }
    SpaceCubemap(const SpaceCubemap&) = delete;
    SpaceCubemap& operator=(const SpaceCubemap&) = delete;
    void bind(unsigned int unit) const {
        // 纹理单元是采样器的槽位；shader.setInt 里传的也是这个槽位编号。
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, id_);
    }
private:
    GLuint id_ = 0;
};

// 本场景的构造和绘制都会改变绑定；保存后恢复，避免影响场景切换和 ImGui。
struct State {
    GLint vao, buffer, program, active, texture, function;
    GLboolean depth=glIsEnabled(GL_DEPTH_TEST), blend=glIsEnabled(GL_BLEND),
              cull=glIsEnabled(GL_CULL_FACE), mask;
    State() {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao); glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);
        glGetIntegerv(GL_CURRENT_PROGRAM,&program); glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glGetIntegerv(GL_DEPTH_FUNC,&function); glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
    }
    ~State() {
        glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,buffer); glUseProgram(program);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture); glActiveTexture(active);
        glDepthFunc(function); glDepthMask(mask);
        restore(GL_DEPTH_TEST,depth); restore(GL_BLEND,blend); restore(GL_CULL_FACE,cull);
    }
    static void restore(GLenum cap,GLboolean enabled) { if (enabled) glEnable(cap); else glDisable(cap); }
};

struct Vertex { glm::vec3 position, normal; glm::vec2 uv; };

// 这里只加载仓库中已知的单材质 planet/rock 模型，不替代支持任意材质的 Model viewer。
// 两个 OBJ 的所有子网格共用同一贴图，合并为一个 VAO 后每种模型只需一次绘制提交。
class InstancedMesh {
public:
    explicit InstancedMesh(const std::string& path) {
        Assimp::Importer importer;
        const aiScene* scene=importer.ReadFile(path,aiProcess_Triangulate|aiProcess_GenSmoothNormals|
            aiProcess_JoinIdenticalVertices|aiProcess_PreTransformVertices);
        if (!scene || !scene->HasMeshes())
            throw std::runtime_error("Cannot load asteroid-belt model: "+path+": "+importer.GetErrorString());
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        glm::vec3 low(std::numeric_limits<float>::max()),high(std::numeric_limits<float>::lowest());
        for (unsigned int m=0;m<scene->mNumMeshes;++m) {
            const aiMesh& mesh=*scene->mMeshes[m];
            const auto base=static_cast<unsigned int>(vertices.size());
            for (unsigned int i=0;i<mesh.mNumVertices;++i) {
                const auto& p=mesh.mVertices[i];
                const auto n=mesh.HasNormals() ? mesh.mNormals[i] : aiVector3D(0,1,0);
                const auto uv=mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][i] : aiVector3D();
                Vertex v{{p.x,p.y,p.z},{n.x,n.y,n.z},{uv.x,uv.y}};
                low=glm::min(low,v.position); high=glm::max(high,v.position);
                vertices.push_back(v);
            }
            for (unsigned int f=0;f<mesh.mNumFaces;++f) {
                const aiFace& face=mesh.mFaces[f];
                if (face.mNumIndices==3)
                    for (unsigned int i=0;i<3;++i) indices.push_back(base+face.mIndices[i]);
            }
        }
        if (vertices.empty() || indices.empty()) throw std::runtime_error("Asteroid-belt model has no triangles: "+path);
        // 将不同导出单位的模型都居中并缩放到最大半径 1，之后的实例缩放才具有一致含义。
        const glm::vec3 center=(low+high)*.5f;
        float radius=0;
        for (const auto& v : vertices) radius=std::max(radius,glm::length(v.position-center));
        if (!(radius>0) || !std::isfinite(radius)) throw std::runtime_error("Invalid asteroid-belt model bounds");
        for (auto& v : vertices) v.position=(v.position-center)/radius;
        count_=static_cast<GLsizei>(indices.size());
        glGenVertexArrays(1,&vao_); glGenBuffers(1,&vbo_); glGenBuffers(1,&ebo_);
        glGenBuffers(1,&instances_);
        glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned int),indices.data(),GL_STATIC_DRAW);
        const size_t offsets[]={offsetof(Vertex,position),offsetof(Vertex,normal),offsetof(Vertex,uv)};
        for (unsigned int i=0;i<3;++i) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i,i==2 ? 2 : 3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsets[i]));
        }
        glBindBuffer(GL_ARRAY_BUFFER,instances_);
        // GLSL 的 mat4 占用连续 4 个属性位置（3..6），每列是一个 vec4。
        for (unsigned int column=0;column<4;++column) {
            glEnableVertexAttribArray(3+column);
            glVertexAttribPointer(3+column,4,GL_FLOAT,GL_FALSE,sizeof(glm::mat4),
                reinterpret_cast<void*>(column*sizeof(glm::vec4)));
            // divisor=1 表示每个“实例”才前进一份矩阵；默认 0 则每个“顶点”前进。
            glVertexAttribDivisor(3+column,1);
        }
    }
    ~InstancedMesh() {
        glDeleteVertexArrays(1,&vao_); glDeleteBuffers(1,&vbo_);
        glDeleteBuffers(1,&ebo_); glDeleteBuffers(1,&instances_);
    }
    InstancedMesh(const InstancedMesh&)=delete;
    InstancedMesh& operator=(const InstancedMesh&)=delete;
    void upload(const std::vector<glm::mat4>& matrices) const {
        glBindBuffer(GL_ARRAY_BUFFER,instances_);
        // 矩阵只在数量/分布改变时重新上传，公转通过共享 uniform 实现，不逐帧重传所有实例。
        glBufferData(GL_ARRAY_BUFFER,matrices.size()*sizeof(glm::mat4),matrices.data(),GL_STATIC_DRAW);
    }
    void draw() const {
        glBindVertexArray(vao_); glDrawElements(GL_TRIANGLES,count_,GL_UNSIGNED_INT,nullptr);
    }
    void drawInstanced(int amount) const {
        glBindVertexArray(vao_);
        glDrawElementsInstanced(GL_TRIANGLES,count_,GL_UNSIGNED_INT,nullptr,amount);
    }
private:
    GLuint vao_=0,vbo_=0,ebo_=0,instances_=0;
    GLsizei count_=0;
};

class AsteroidBeltScene final : public CameraScene {
public:
    AsteroidBeltScene(const std::string& root,int amount,bool instanced)
        : shader_(loadShader(root,"instancing","asteroid")),
          skyShader_(loadShader(root,"instancing","skybox")),
          sky_(std::make_unique<Mesh>(skyboxVertices(),std::initializer_list<int>{3})),
          space_(root),
          planet_(root+"/resources/models/planet/planet.obj"),
          rock_(root+"/resources/models/rock/rock.obj"),
          planetTexture_(root+"/resources/models/planet/mars.png"),
          rockTexture_(root+"/resources/models/rock/rock.png"),
          amount_(std::clamp(amount,0,10000)),instanced_(instanced) {
        // 普通绘制的 VAO 同样具有矩阵属性，准备一份有效数据供属性读取。
        planet_.upload({glm::mat4(1)});
        regenerate(); resetCamera();
        shader_->use(); shader_->setInt("surfaceTexture",0);
        // 采样器 uniform 保存的是纹理单元编号（0），不是纹理 ID；每帧只需重新绑定纹理。
        skyShader_->use(); skyShader_->setInt("spaceMap",0);
    }
    void render(const SceneFrame& frame) override {
        State previous;
        if (dirty_) { regenerate(); dirty_=false; }
        if (animate_) angle_=std::fmod(angle_+std::max(0.f,frame.deltaTime)*speed_,360.f);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
        glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
        // 立方体贴图相邻面之间做无缝过滤，否则天空盒边缘可能出现细线。
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        // 先画天空盒、再画不透明物体，可以避免大量被遮挡的片元仍然执行着色。
        // 天空盒写入的深度是 1.0（顶点着色器把 z 设为 w），物体只要比远平面更近就会覆盖它。
        drawSky(frame);
        shader_->use(); shader_->setMat4("view",camera_.GetViewMatrix());
        shader_->setMat4("projection",glm::perspective(glm::radians(camera_.Zoom),frame.aspect,.1f,150.f));
        shader_->setMat4("orbit",glm::mat4(1)); shader_->setInt("instanced",false);
        shader_->setMat4("model",glm::scale(glm::mat4(1),glm::vec3(2.6f)));
        planetTexture_.bind(0); planet_.draw();
        rockTexture_.bind(0);
        shader_->setMat4("orbit",glm::rotate(glm::mat4(1),glm::radians(angle_),glm::vec3(0,1,0)));
        shader_->setInt("instanced",instanced_);
        if (instanced_) rock_.drawInstanced(amount_);
        else for (const auto& model : matrices_) {
            shader_->setMat4("model",model); rock_.draw();
        }
    }
    void drawControls() override {
        ImGui::Checkbox("Use instanced drawing",&instanced_);
        dirty_|=ImGui::SliderInt("Asteroid count",&amount_,0,10000);
        dirty_|=ImGui::SliderFloat("Belt radius",&radius_,6.f,14.f,"%.1f");
        dirty_|=ImGui::SliderFloat("Belt half-width",&width_,.5f,3.f,"%.1f");
        ImGui::Checkbox("Animate orbit",&animate_);
        ImGui::SliderFloat("Orbit speed (deg/s)",&speed_,0.f,20.f,"%.1f");
        ImGui::Text("Draw calls: %d (planet included)",1+(instanced_ ? (amount_>0 ? 1 : 0) : amount_));
        ImGui::Text("Instance matrices: %.1f KiB",amount_*sizeof(glm::mat4)/1024.f);
        ImGui::TextWrapped("One rock mesh, many model matrices. Locations 3-6 hold a mat4; divisor 1 advances it once per instance, not per vertex.");
        ImGui::TextWrapped("Compare identical layouts with instancing ON/OFF. Instancing reduces CPU draw submissions, not the number of triangles or shaded fragments.");
        ImGui::TextWrapped("WASD / right drag / wheel control the camera. The static random seed keeps comparisons repeatable; orbit animation is off by default.");
        ImGui::TextWrapped("The starfield is a skybox: a cube whose vertex positions double as cubemap sampling directions, drawn with depth z=w so it always sits at the far plane.");
        if (ImGui::Button("Reset belt")) {
            amount_=2000; radius_=10; width_=2; instanced_=true; animate_=false;
            angle_=0; speed_=5; dirty_=true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset camera")) resetCamera();
    }
private:
    // 立方体顶点以原点为中心、边长为 2。顶点位置被天空盒顶点着色器当作采样方向使用。
    static std::vector<float> skyboxVertices() {
        return {
            -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1, // 背面
            -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1, // 正面
            -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1, // 左面
             1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1, // 右面
            -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1, // 底面
            -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1, // 顶面
        };
    }
    void drawSky(const SceneFrame& frame) {
        const auto view=camera_.GetViewMatrix();
        // glDepthMask(GL_FALSE) 后所有物体（包括后面要画的行星）都不再写深度；
        // 天空盒画完必须立刻恢复写入，否则小行星之间会互相穿透、遮挡关系全错。
        glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        skyShader_->use();
        // 只保留观察矩阵的旋转部分（mat3 去掉平移列），天空盒就永远以相机为中心，看起来无限远。
        skyShader_->setMat4("view",glm::mat4(glm::mat3(view)));
        // 远平面与物体共用：天空盒深度恒为 1，只要远平面比最远小行星更远，球壳就不会被裁掉。
        skyShader_->setMat4("projection",glm::perspective(glm::radians(camera_.Zoom),frame.aspect,.1f,150.f));
        space_.bind(0);
        sky_->draw();
        // 恢复成常规的不透明绘制状态，同时把深度测试恢复为 LESS，供后面的物体使用。
        glDepthFunc(GL_LESS); glDepthMask(GL_TRUE);
    }
    void resetCamera() {
        camera_=Camera(glm::vec3(0,18,34),glm::vec3(0,1,0),-90.f,-28.f);
        camera_.MovementSpeed=10;
    }
    void regenerate() {
        std::mt19937 random(2026); // 不使用全局 rand，不影响其他场景，切换绘制模式时布局不变。
        std::uniform_real_distribution<float> unit(0.f,1.f);
        matrices_.clear(); matrices_.reserve(amount_);
        for (int i=0;i<amount_;++i) {
            const float angle=6.28318530718f*i/amount_;
            const float r=radius_+(unit(random)*2-1)*width_;
            const float y=(unit(random)*2-1)*.65f;
            const float scale=.08f+unit(random)*.14f;
            auto model=glm::translate(glm::mat4(1),glm::vec3(std::sin(angle)*r,y,std::cos(angle)*r));
            model=glm::rotate(model,unit(random)*6.28318530718f,glm::normalize(glm::vec3(.4f,.6f,.8f)));
            matrices_.push_back(glm::scale(model,glm::vec3(scale)));
        }
        rock_.upload(matrices_);
    }
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Shader> skyShader_;
    std::unique_ptr<Mesh> sky_;
    SpaceCubemap space_;
    InstancedMesh planet_,rock_;
    Texture planetTexture_,rockTexture_;
    std::vector<glm::mat4> matrices_;
    int amount_=2000;
    bool instanced_=true,animate_=false,dirty_=false;
    float radius_=10,width_=2,angle_=0,speed_=5;
};
} // namespace

std::unique_ptr<Scene> createAsteroidBeltScene(const std::string& root,int amount,bool instanced) {
    State previous;
    return std::make_unique<AsteroidBeltScene>(root,amount,instanced);
}
