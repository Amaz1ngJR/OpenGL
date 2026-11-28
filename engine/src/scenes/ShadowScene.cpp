#include "SceneFactories.h"
#include "SceneModel.h"
#include "SceneSupport.h"
#include <array>
#include <cmath>

using namespace sceneSupport;
namespace {
constexpr int planarSize = 1024, pointSize = 512, areaSamples = 4;
constexpr int spotLayer = 1 + areaSamples;
constexpr float pointFar = 25.f;
const glm::vec3 defaultPointPosition{-2.6f,2.5f,2.0f}, defaultAreaPosition{2.1f,3.1f,.8f},
                defaultSpotPosition{-.7f,2.1f,-1.9f};
constexpr float defaultSpotAzimuth = 75.f, defaultSpotTilt = 44.f;

// 同一个 FBO 依次挂接平行光、面光源、聚光灯的图层和点光源立方体的六个面。
class ShadowMaps {
public:
    ShadowMaps() {
        glGenFramebuffers(1, &fbo_);
        glGenTextures(1, &planar_);
        glGenTextures(1, &cube_);
        GLint planarBinding, cubeBinding;
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &planarBinding);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &cubeBinding);
        glBindTexture(GL_TEXTURE_2D_ARRAY, planar_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, planarSize, planarSize,
                     spotLayer + 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        sampling(GL_TEXTURE_2D_ARRAY);
        // 线性过滤深度比较的结果，而不是插值遮挡物深度。
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube_);
        for (int face = 0; face < 6; ++face)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24,
                         pointSize, pointSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        sampling(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D_ARRAY, planarBinding);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeBinding);
        GLint drawFramebuffer, readFramebuffer;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        begin();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
        glReadBuffer(GL_NONE);
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, planar_, 0, 0);
        const auto planarStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cube_, 0);
        const auto pointStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        if (planarStatus != GL_FRAMEBUFFER_COMPLETE || pointStatus != GL_FRAMEBUFFER_COMPLETE) {
            release();
            throw std::runtime_error("Shadow depth framebuffer is incomplete");
        }
    }
    ~ShadowMaps() { release(); }
    ShadowMaps(const ShadowMaps&) = delete;
    ShadowMaps& operator=(const ShadowMaps&) = delete;
    void begin() const {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);
        glDrawBuffer(GL_NONE);
    }
    void attachPlanar(int layer) const {
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, planar_, 0, layer);
        clear(planarSize);
    }
    void attachPoint(int face) const {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cube_, 0);
        clear(pointSize);
    }
    void bindTextures() const {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, planar_);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube_);
    }
private:
    void release() const {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteTextures(1, &planar_);
        glDeleteTextures(1, &cube_);
    }
    static void sampling(GLenum target) {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    static void clear(int size) {
        const float farDepth = 1.f;
        glViewport(0, 0, size, size);
        glClearBufferfv(GL_DEPTH, 0, &farDepth);
    }
    GLuint fbo_ = 0, planar_ = 0, cube_ = 0;
};

struct RenderState {
    GLint framebuffer, viewport[4], depthFunc, activeTexture, vao;
    GLint albedoBinding, planarBinding, pointBinding;
    std::array<GLint,4> wallBindings;
    GLboolean depthMask;
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE),
              blend = glIsEnabled(GL_BLEND), scissor = glIsEnabled(GL_SCISSOR_TEST);
    RenderState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &albedoBinding);
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &planarBinding);
        glActiveTexture(GL_TEXTURE2);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &pointBinding);
        for (unsigned i = 0; i < wallBindings.size(); ++i) {
            glActiveTexture(GL_TEXTURE3 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &wallBindings[i]);
        }
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
    }
    void restoreTarget() const {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        if (scissor) glEnable(GL_SCISSOR_TEST);
    }
    ~RenderState() {
        restoreTarget();
        glDepthMask(depthMask);
        glDepthFunc(depthFunc);
        if (!depth) glDisable(GL_DEPTH_TEST);
        if (cull) glEnable(GL_CULL_FACE);
        if (blend) glEnable(GL_BLEND);
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, albedoBinding);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, planarBinding);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, pointBinding);
        for (unsigned i = 0; i < wallBindings.size(); ++i) {
            glActiveTexture(GL_TEXTURE3 + i);
            glBindTexture(GL_TEXTURE_2D, wallBindings[i]);
        }
        glActiveTexture(activeTexture);
    }
};

struct Light {
    glm::vec3 color;
    float intensity;
    bool enabled = true;

    bool controls() {
        const bool enabledChanged = ImGui::Checkbox("Enabled", &enabled);
        ImGui::ColorEdit3("Color", &color.x);
        const bool wasOff = intensity <= 0;
        ImGui::SliderFloat("Intensity", &intensity, 0.f, 16.f);
        return enabledChanged || (wasOff && intensity > 0);
    }
    glm::vec3 radiance() const { return enabled ? color * intensity : glm::vec3(0); }
};

const glm::mat4 identity(1.f);
glm::mat4 place(glm::vec3 position, glm::vec3 size) {
    return glm::scale(glm::translate(identity, position), size);
}
// 截锥前端在 z=0，后端朝 +Z；局部 -Z 为出光方向。
std::vector<float> spotFixtureVertices() {
    constexpr int segments = 24;
    std::vector<float> vertices;
    auto point = [](int i) {
        const float a = glm::radians(360.f) * i / segments;
        return glm::vec3(std::cos(a), std::sin(a), 0.f);
    };
    auto emit = [&](glm::vec3 p) { vertices.insert(vertices.end(), {p.x,p.y,p.z}); };
    for (int i = 0; i < segments; ++i) {
        const auto a = point(i), b = point((i + 1) % segments);
        const auto c = a * .72f + glm::vec3(0,0,1), d = b * .72f + glm::vec3(0,0,1);
        emit(a); emit(b); emit(c);
        emit(b); emit(d); emit(c);
        emit(glm::vec3(0)); emit(b); emit(a);
        emit(glm::vec3(0,0,1)); emit(c); emit(d);
    }
    return vertices;
}

class ShadowScene final : public CameraScene {
public:
    explicit ShadowScene(const std::string& root)
        : surface_(loadShader(root, "shadows", "surface")), depth_(loadShader(root, "shadows", "depth")),
          marker_(loadShader(root, "lighting/light-source", "light_cube")),
          floor_({-6,0,-6, 0,1,0, 0,6, -6,0,6, 0,1,0, 0,0, 6,0,6, 0,1,0, 6,0,
                  -6,0,-6, 0,1,0, 0,6, 6,0,6, 0,1,0, 6,0, 6,0,-6, 0,1,0, 6,6}, {3,3,2}),
          // 底边与地板左边共用两个端点；U 朝 -Z、V 朝 +Y，每 2 个世界单位平铺一次。
          wall_({-6,0,6, 1,0,0, 0,0, -6,0,-6, 1,0,0, 6,0,
                 -6,5,-6, 1,0,0, 6,2.5f, -6,5,6, 1,0,0, 0,2.5f}, {3,3,2}, {0,1,2,0,2,3}),
          cube_(cubeVertices(true, true), {3,3,2}),
          // 箭头底面位于 z=0，尖端朝局部 -Z，与出光方向一致。
          arrowHead_({-.5f,-.5f,0, .5f,-.5f,0, .5f,.5f,0, -.5f,.5f,0, 0,0,-1}, {3},
                     {0,1,2, 0,2,3, 1,0,4, 2,1,4, 3,2,4, 0,3,4}),
          spotFixture_(spotFixtureVertices(), {3}),
          wood_(root + "/resources/textures/wood.png"), box_(root + "/resources/textures/container2.png"),
          wallTexture_(root + "/resources/textures/pbr/wall/albedo.png"),
          wallNormal_(root + "/resources/textures/pbr/wall/normal.png"),
          wallRoughness_(root + "/resources/textures/pbr/wall/roughness.png"),
          wallMetallic_(root + "/resources/textures/pbr/wall/metallic.png"),
          wallAO_(root + "/resources/textures/pbr/wall/ao.png"),
          smiling_(loadSceneModel(root + u8"/resources/models/水豚噜噜_傻笑/傻笑噜噜.glb", false)),
          waving_(loadSceneModel(root + u8"/resources/models/水豚噜噜_挥手带骨骼/水豚噜噜.glb", true)),
          blueCar_(loadSceneModel(root + "/resources/models/blue_car_lulu.glb", false)),
          waveDuration_(waving_->duration(0)) {
        camera_ = Camera(glm::vec3(0, 3.05f, 7.8f), glm::vec3(0,1,0), -90.f, -15.f);
    }

    void render(const SceneFrame& frame) override {
        RenderState state;
        // 同一姿势用于所有深度 pass 和主画面；复用现有 CPU 蒙皮，每秒最多更新约 24 次。
        if (playWave_ && waveDuration_ > 0) {
            waveTime_ += frame.deltaTime;
            if (waveTime_ - lastPoseTime_ >= 1.0 / 24.0) {
                waving_->update(0, std::fmod(waveTime_, waveDuration_));
                lastPoseTime_ = waveTime_;
                mapsDirty_ = true;
            }
        }
        const float azimuth = glm::radians(sunAzimuth_), elevation = glm::radians(sunElevation_);
        const glm::vec3 toSun(std::cos(elevation)*std::cos(azimuth), std::sin(elevation),
                              std::cos(elevation)*std::sin(azimuth));
        const glm::mat4 sunMatrix = glm::ortho(-9.f,9.f,-9.f,9.f,.1f,23.f) *
            glm::lookAt(toSun * 11.f, glm::vec3(0), glm::vec3(0,1,0));
        const float spotAzimuth = glm::radians(spotAzimuth_), spotTilt = glm::radians(spotTilt_);
        const glm::vec3 spotDirection(std::cos(spotTilt)*std::cos(spotAzimuth), -std::sin(spotTilt),
                                     std::cos(spotTilt)*std::sin(spotAzimuth));
        const auto spotView = glm::lookAt(spotPosition_, spotPosition_ + spotDirection, glm::vec3(0,1,0));
        // 外锥角为半角；额外留出边缘 PCF 的采样空间。
        const auto spotMatrix = glm::perspective(glm::radians(2.f * (spotOuter_ + 1.f)), 1.f, .08f, 25.f) * spotView;
        std::array<glm::vec3, areaSamples> emitters;
        std::array<glm::mat4, areaSamples> areaMatrices;
        for (int i = 0; i < areaSamples; ++i) {
            // 四个分层采样点各代表矩形的一个象限，面积变大时半影随之扩大。
            emitters[i] = areaPosition_ + glm::vec3((i & 1 ? .25f : -.25f) * areaWidth_, 0,
                                                  (i & 2 ? .25f : -.25f) * areaLength_);
            areaMatrices[i] = glm::perspective(glm::radians(165.f), 1.f, .12f, 24.f) *
                glm::lookAt(emitters[i], emitters[i] + glm::vec3(0,-1,0), glm::vec3(0,0,-1));
        }
        if (shadows_ && mapsDirty_) {
            glDisable(GL_SCISSOR_TEST);
            renderShadows(sunMatrix, areaMatrices, spotMatrix);
            state.restoreTarget();
            mapsDirty_ = false;
        }

        const auto view = camera_.GetViewMatrix(), proj = projection(frame);
        surface_->use();
        surface_->setMat4("view", view);
        surface_->setMat4("projection", proj);
        surface_->setVec3("viewPos", camera_.Position);
        surface_->setInt("albedo", 0);
        surface_->setInt("planarDepth", 1);
        surface_->setInt("pointDepth", 2);
        surface_->setInt("wallNormal", 3);
        surface_->setInt("wallRoughness", 4);
        surface_->setInt("wallMetallic", 5);
        surface_->setInt("wallAO", 6);
        surface_->setInt("useShadows", shadows_);
        surface_->setVec3("toSun", toSun);
        surface_->setVec3("sunColor", sun_.radiance());
        surface_->setVec3("pointPosition", pointPosition_);
        surface_->setVec3("pointColor", point_.radiance());
        surface_->setFloat("pointFar", pointFar);
        surface_->setVec3("areaColor", area_.radiance());
        surface_->setMat4("sunMatrix", sunMatrix);
        surface_->setVec3("spotPosition", spotPosition_);
        surface_->setVec3("spotDirection", spotDirection);
        surface_->setVec3("spotColor", spot_.radiance());
        surface_->setFloat("spotInnerCos", std::cos(glm::radians(spotInner_)));
        surface_->setFloat("spotOuterCos", std::cos(glm::radians(spotOuter_)));
        surface_->setMat4("spotMatrix", spotMatrix);
        surface_->setInt("spotLayer", spotLayer);
        for (int i = 0; i < areaSamples; ++i) {
            const auto index = "[" + std::to_string(i) + "]";
            surface_->setVec3("emitters" + index, emitters[i]);
            surface_->setMat4("areaMatrices" + index, areaMatrices[i]);
        }
        maps_.bindTextures();
        surface_->setVec3("baseColor", glm::vec3(1));
        surface_->setMat4("model", identity);
        wood_.bind(0);
        floor_.draw();
        drawObjects(*surface_, true);

        if (showLights_) {
            marker_->use();
            marker_->setMat4("view", view);
            marker_->setMat4("projection", proj);
            if (point_.enabled) drawMarker(cube_, point_.color, place(pointPosition_, glm::vec3(.10f)));
            if (area_.enabled) {
                // 深色边框与背板包住朝下的发光面；两者随面光源尺寸一起变化。
                drawMarker(cube_, {.18f,.20f,.24f}, place(areaPosition_ + glm::vec3(0,.04f,0),
                                                       {areaWidth_, .08f, areaLength_}));
                drawMarker(cube_, area_.color, place(areaPosition_ - glm::vec3(0,.01f,0),
                                                     {areaWidth_ * .92f, .02f, areaLength_ * .92f}));
            }
            if (sun_.enabled) {
                // 这只是方向标记；三支箭头共用姿态，沿 -toSun 指向场景。
                const auto mount = glm::inverse(glm::lookAt(toSun * 4.f, glm::vec3(0), glm::vec3(0,1,0)));
                for (float x : {-.32f, 0.f, .32f}) {
                    drawMarker(cube_, sun_.color, mount * place({x,0,.18f}, {.055f,.055f,.36f}));
                    drawMarker(arrowHead_, sun_.color, mount * place({x,0,0}, {.20f,.20f,.23f}));
                }
            }
            if (spot_.enabled) {
                const auto mount = glm::inverse(spotView);
                drawMarker(spotFixture_, {.23f,.26f,.31f}, mount * place({0,0,.08f}, {.23f,.23f,.42f}));
                drawMarker(spotFixture_, {.10f,.12f,.16f}, mount * place({0,0,.008f}, {.25f,.25f,.10f}));
                drawMarker(spotFixture_, spot_.color, mount * place({0,0,0}, {.19f,.19f,.02f}));
            }
        }
    }

    void drawControls() override {
        ImGui::Checkbox("Cast shadows", &shadows_);
        ImGui::Checkbox("Show light markers", &showLights_);
        ImGui::Checkbox("Play Lulu's wave", &playWave_);
        if (ImGui::CollapsingHeader("Spotlight", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("Spot");
            mapsDirty_ |= spot_.controls();
            mapsDirty_ |= positionControls(spotPosition_, defaultSpotPosition);
            mapsDirty_ |= ImGui::SliderFloat("Azimuth", &spotAzimuth_, -180.f, 180.f, "%.0f deg");
            mapsDirty_ |= ImGui::SliderFloat("Downward tilt", &spotTilt_, 5.f, 89.f, "%.0f deg");
            if (ImGui::Button("Reset direction")) {
                spotAzimuth_ = defaultSpotAzimuth;
                spotTilt_ = defaultSpotTilt;
                mapsDirty_ = true;
            }
            ImGui::SliderFloat("Inner half-angle", &spotInner_, 1.f, spotOuter_ - 1.f, "%.0f deg");
            mapsDirty_ |= ImGui::SliderFloat("Outer half-angle", &spotOuter_, spotInner_ + 1.f, 65.f, "%.0f deg");
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Point light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("Point");
            mapsDirty_ |= point_.controls();
            mapsDirty_ |= positionControls(pointPosition_, defaultPointPosition);
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Rectangular area light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("Area");
            mapsDirty_ |= area_.controls();
            mapsDirty_ |= positionControls(areaPosition_, defaultAreaPosition);
            mapsDirty_ |= ImGui::SliderFloat("Width", &areaWidth_, .2f, 4.f);
            mapsDirty_ |= ImGui::SliderFloat("Length", &areaLength_, .2f, 4.f);
            ImGui::PopID();
        }
        if (ImGui::CollapsingHeader("Directional light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("Sun");
            mapsDirty_ |= sun_.controls();
            mapsDirty_ |= ImGui::SliderFloat("Azimuth", &sunAzimuth_, -180.f, 180.f, "%.0f deg");
            mapsDirty_ |= ImGui::SliderFloat("Elevation", &sunElevation_, 5.f, 89.f, "%.0f deg");
            ImGui::PopID();
        }
    }

private:
    static bool positionControls(glm::vec3& position, const glm::vec3& defaultPosition) {
        bool changed = ImGui::SliderFloat("X", &position.x, -6.f, 6.f);
        changed |= ImGui::SliderFloat("Height (Y)", &position.y, .5f, 7.f);
        changed |= ImGui::SliderFloat("Z", &position.z, -6.f, 6.f);
        if (ImGui::Button("Reset position")) {
            position = defaultPosition;
            changed = true;
        }
        return changed;
    }
    void drawMarker(const Mesh& mesh, glm::vec3 color, const glm::mat4& model) const {
        marker_->setVec3("lightColor", color);
        marker_->setMat4("model", model);
        mesh.draw();
    }
    void drawObjects(Shader& shader, bool materials) const {
        shader.setMat4("model", identity);
        if (materials) {
            shader.setVec3("baseColor", glm::vec3(1));
            shader.setInt("useWallMaterial", true);
            wallTexture_.bind(0);
            wallNormal_.bind(3);
            wallRoughness_.bind(4);
            wallMetallic_.bind(5);
            wallAO_.bind(6);
        }
        wall_.draw();
        if (materials) {
            shader.setInt("useWallMaterial", false);
            box_.bind(0);
        }
        shader.setMat4("model", place({-.05f,.58f,-.65f}, glm::vec3(1.16f)));
        cube_.draw();
        shader.setMat4("model", place({.45f,.34f,1.1f}, glm::vec3(.68f)));
        cube_.draw();
        smiling_->drawPlaced(shader, place({-1.85f,0,.05f}, glm::vec3(1)), materials);
        waving_->drawPlaced(shader, place({1.85f,0,-.1f}, glm::vec3(1)), materials);
        blueCar_->drawPlaced(shader, glm::rotate(place({-.8f,0,2.45f}, glm::vec3(.78f)),
                                                 glm::radians(-25.f), glm::vec3(0,1,0)), materials);
    }
    void renderShadows(const glm::mat4& sunMatrix, const std::array<glm::mat4, areaSamples>& areaMatrices,
                       const glm::mat4& spotMatrix) const {
        maps_.begin();
        depth_->use();
        depth_->setInt("radial", 0);
        if (sun_.enabled && sun_.intensity > 0) {
            maps_.attachPlanar(0);
            depth_->setMat4("lightMatrix", sunMatrix);
            drawObjects(*depth_, false);
        }
        if (area_.enabled && area_.intensity > 0) for (int i = 0; i < areaSamples; ++i) {
            maps_.attachPlanar(i + 1);
            depth_->setMat4("lightMatrix", areaMatrices[i]);
            drawObjects(*depth_, false);
        }
        if (spot_.enabled && spot_.intensity > 0) {
            maps_.attachPlanar(spotLayer);
            depth_->setMat4("lightMatrix", spotMatrix);
            drawObjects(*depth_, false);
        }
        if (point_.enabled && point_.intensity > 0) {
            const glm::vec3 axes[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            const glm::vec3 ups[] = {{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
            const auto proj = glm::perspective(glm::radians(90.f), 1.f, .08f, pointFar);
            depth_->setInt("radial", 1);
            depth_->setVec3("pointPosition", pointPosition_);
            depth_->setFloat("pointFar", pointFar);
            for (int face = 0; face < 6; ++face) {
                maps_.attachPoint(face);
                depth_->setMat4("lightMatrix", proj * glm::lookAt(pointPosition_, pointPosition_ + axes[face], ups[face]));
                drawObjects(*depth_, false);
            }
        }
    }

    std::unique_ptr<Shader> surface_, depth_, marker_;
    Mesh floor_, wall_, cube_, arrowHead_, spotFixture_;
    Texture wood_, box_, wallTexture_, wallNormal_, wallRoughness_, wallMetallic_, wallAO_;
    std::unique_ptr<SceneModel> smiling_, waving_, blueCar_;
    ShadowMaps maps_;
    double waveDuration_, waveTime_ = 0, lastPoseTime_ = -1;
    Light point_{{1.f,.78f,.58f}, 5.f}, area_{{.63f,.78f,1.f}, 7.f}, sun_{{1.f,.95f,.87f}, .85f};
    Light spot_{{.58f,.87f,1.f}, 10.f};
    glm::vec3 pointPosition_ = defaultPointPosition, areaPosition_ = defaultAreaPosition,
              spotPosition_ = defaultSpotPosition;
    float areaWidth_ = 2.f, areaLength_ = 1.5f, sunAzimuth_ = -55.f, sunElevation_ = 43.f;
    float spotAzimuth_ = defaultSpotAzimuth, spotTilt_ = defaultSpotTilt, spotInner_ = 18.f, spotOuter_ = 32.f;
    bool shadows_ = true, showLights_ = true, playWave_ = true, mapsDirty_ = true;
};
} // namespace

std::unique_ptr<Scene> createShadowScene(const std::string& root) {
    return std::make_unique<ShadowScene>(root);
}
