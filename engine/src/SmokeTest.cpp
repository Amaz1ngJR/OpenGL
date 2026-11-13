#include "SmokeTest.h"
#include "Scene.h"
#include "ModelScene.h"
#include "scenes/SceneFactories.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
// 通过真实 ImGui 鼠标事件切换光照控件的首个复选框，覆盖暂停与恢复路径。
void toggleLightPause(Scene& scene) {
    ImVec2 checkbox;
    for (int step = 0; step < 4; ++step) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        if (step > 0) ImGui::GetIO().AddMousePosEvent(checkbox.x, checkbox.y);
        if (step == 2) ImGui::GetIO().AddMouseButtonEvent(0, true);
        if (step == 3) ImGui::GetIO().AddMouseButtonEvent(0, false);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(360, 500));
        ImGui::Begin("Pause control test");
        checkbox = ImGui::GetCursorScreenPos();
        checkbox.x += 5;
        checkbox.y += 5;
        scene.drawControls();
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
}

// 在真实 OpenGL 上下文下反复创建/绘制/销毁所有场景，检查 GL 错误和非空画面。
void smokeTest(GLFWwindow* window, const std::string& root, const std::string& modelPath) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) throw std::runtime_error("No drawable framebuffer");
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    for (int round = 0; round < 3; ++round) {
        for (int index = 0; index < static_cast<int>(availableScenes().size()); ++index) {
            auto scene = index == 1 && !modelPath.empty() ? createModelScene(root, modelPath) : createScene(index, root);
            glViewport(0, 0, width, height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            scene->render({1.0f, .016f, static_cast<float>(width) / height});
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            size_t colored = 0;
            for (size_t p = 0; p < pixels.size(); p += 4)
                if (pixels[p] || pixels[p + 1] || pixels[p + 2]) ++colored;
            // 模型预览尚未选文件时与空场景一样仅清屏，不伪造默认模型。
            const bool emptyScene = index == 0 || (index == 1 && modelPath.empty());
            if ((emptyScene && colored != 0) || (!emptyScene && colored == 0))
                throw std::runtime_error(std::string("Unexpected framebuffer: ") + availableScenes()[index].name);
            if (index == 7 || index == 8) {
                // 不只检查是否有像素，还要检查深度卡片和模板描边的教学结果。
                size_t orange = 0, green = 0, blue = 0, yellow = 0;
                for (size_t p = 0; p < pixels.size(); p += 4) {
                    const int r = pixels[p], g = pixels[p+1], b = pixels[p+2];
                    if (r > 240 && g > 70 && g < 110 && b < 40) ++orange;
                    if (r < 50 && g > 210 && b < 110) ++green;
                    if (r < 50 && g > 95 && g < 135 && b > 240) ++blue;
                    if (r > 240 && g > 145 && g < 185 && b < 40) ++yellow;
                }
                if (index == 7 && (orange == 0 || green != 0 || blue != 0))
                    throw std::runtime_error("LESS must show the closer orange card and reject equal/farther cards");
                if (index == 8) {
                    std::vector<unsigned char> stencilPixels(static_cast<size_t>(width) * height);
                    glReadPixels(0, 0, width, height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilPixels.data());
                    size_t marked = 0;
                    for (auto value : stencilPixels) if (value == 1) ++marked;
                    if (yellow == 0 || marked == 0)
                        throw std::runtime_error("Stencil outline or reference mask missing");
                }
            }
            if (index == 6) {
                // 先在 time=1 时绘制并记下光源矩阵与颜色。
                scene->render({1.0f, .016f, static_cast<float>(width) / height});
                GLint program = 0;
                glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                const GLint modelLocation = glGetUniformLocation(program, "model");
                const GLint colorLocation = glGetUniformLocation(program, "lightColor");
                GLfloat previousModel[16]{}, previousColor[3]{};
                glGetUniformfv(program, modelLocation, previousModel);
                glGetUniformfv(program, colorLocation, previousColor);

                toggleLightPause(*scene);
                for (float time : {2.0f, 3.0f}) {
                    scene->render({time, .016f, static_cast<float>(width) / height});
                    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                    GLfloat model[16]{}, color[3]{};
                    glGetUniformfv(program, glGetUniformLocation(program, "model"), model);
                    glGetUniformfv(program, glGetUniformLocation(program, "lightColor"), color);
                    for (int component = 0; component < 16; ++component)
                        if (std::abs(model[component] - previousModel[component]) > .001f)
                            throw std::runtime_error("Paused light position changed");
                    for (int component = 0; component < 3; ++component)
                        if (std::abs(color[component] - previousColor[component]) > .001f)
                            throw std::runtime_error("Paused light color changed");
                }
                // 取消暂停后时间恢复推进，模型矩阵中的光源位置应继续绕圈。
                toggleLightPause(*scene);
                scene->render({4.0f, .016f, static_cast<float>(width) / height});
                glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                GLfloat resumedModel[16]{}, resumedColor[3]{};
                glGetUniformfv(program, glGetUniformLocation(program, "model"), resumedModel);
                glGetUniformfv(program, glGetUniformLocation(program, "lightColor"), resumedColor);
                // 暂停两秒不计入动画；恢复这一帧时有效动画时间应是 2 秒，而不是全局 4 秒。
                if (std::abs(resumedModel[12] - std::sin(2.f * .8f) * 2.f) > .001f ||
                    std::abs(resumedModel[14] - std::cos(2.f * .8f) * 2.f) > .001f ||
                    std::abs(resumedColor[0] - (.5f + .5f * std::sin(2.f * 2.f))) > .001f ||
                    std::abs(resumedColor[1] - (.5f + .5f * std::sin(2.f * .7f))) > .001f ||
                    std::abs(resumedColor[2] - (.5f + .5f * std::sin(2.f * 1.3f))) > .001f)
                    throw std::runtime_error("Light animation did not resume from paused time");
            }
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Scene smoke test");
            for (int option = 0; option < static_cast<int>(availableScenes().size()); ++option)
                ImGui::RadioButton(availableScenes()[option].name, option == index);
            scene->drawControls();
            ImGui::End();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            // 删除当前绑定的程序前解绑，避免 ImGui 下一帧恢复已删除的 program。
            glUseProgram(0);
            glBindVertexArray(0);
            scene.reset();
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error(std::string("OpenGL error: ") + availableScenes()[index].name);
            std::cout << "PASS " << round + 1 << ": " << availableScenes()[index].name << '\n';
        }
    }
    // Exercise every post-process shader branch and attachment reallocation.
    std::vector<unsigned char> original(160*90*4), output(original.size());
    for (int effect=0; effect<7; ++effect) {
        auto scene=createFramebufferScene(root,effect);
        for (int size : {80,160}) {
            glViewport(0,0,size,size*9/16);
            scene->render({1.f,.016f,16.f/9.f});
            GLint draw=0,read=0,viewport[4];
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
            glGetIntegerv(GL_VIEWPORT,viewport);
            if (draw!=0 || read!=0 || viewport[2]!=size || viewport[3]!=size*9/16)
                throw std::runtime_error("Framebuffer scene did not restore target/viewport");
        }
        glReadPixels(0,0,160,90,GL_RGBA,GL_UNSIGNED_BYTE,output.data());
        if (effect==0) original=output;
        if (effect==1 || effect==2) {
            for (size_t p=0;p<output.size();p+=4) {
                if (effect==1) {
                    for (int c=0;c<3;++c)
                        if (std::abs(int(output[p+c])+int(original[p+c])-255)>2)
                            throw std::runtime_error("Framebuffer invert output incorrect");
                } else if (output[p]!=output[p+1] || output[p]!=output[p+2])
                    throw std::runtime_error("Framebuffer grayscale output incorrect");
            }
        }
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Framebuffer effect GL error");
        glUseProgram(0); glBindVertexArray(0);
        std::cout << "PASS framebuffer effect + resize: " << effect << '\n';
    }
    // Render all independent reflection/refraction combinations. A skybox-only
    // image would look non-empty too, so require each object's half to change.
    std::vector<unsigned char> reflected(160*90*4), mapped(reflected.size());
    glViewport(0,0,160,90);
    for (int cubeMode=0;cubeMode<2;++cubeMode) for (int sphereMode=0;sphereMode<2;++sphereMode) {
        auto scene=createCubemapScene(root,cubeMode,sphereMode);
        glDepthMask(GL_TRUE); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene->render({1.f,.016f,16.f/9.f});
        glReadPixels(0,0,160,90,GL_RGBA,GL_UNSIGNED_BYTE,mapped.data());
        if (cubeMode==0 && sphereMode==0) reflected=mapped;
        size_t leftChanged=0,rightChanged=0;
        for (int y=0;y<90;++y) for (int x=0;x<160;++x) {
            size_t p=(y*160+x)*4;
            int difference=0;
            for (int c=0;c<3;++c) difference+=std::abs(int(mapped[p+c])-int(reflected[p+c]));
            if (difference>12) { if(x<80) ++leftChanged; else ++rightChanged; }
        }
        if ((cubeMode==1 && leftChanged<20) || (sphereMode==1 && rightChanged<20))
            throw std::runtime_error("Cubemap object reflection/refraction did not change");
        if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Cubemap GL error");
        glUseProgram(0); glBindVertexArray(0);
        std::cout << "PASS cubemap modes: " << cubeMode << ", " << sphereMode << '\n';
    }
    // 不仅检查行星非空：0 实例作为基线，实例化必须确实画出小行星，且与普通绘制一致。
    glViewport(0,0,320,180);
    std::vector<unsigned char> planetOnly(320*180*4),instancedPixels(planetOnly.size()),ordinary(planetOnly.size());
    // 天空盒覆盖整屏，因此"行星基线"的每个像素都应该被星空而非黑色背景占据。
    std::vector<unsigned char> skyOnly(planetOnly.size());
    for (int amount : {0,1,2000}) {
        for (bool instanced : {true,false}) {
            auto scene=createAsteroidBeltScene(root,amount,instanced);
            glClearColor(0,0,0,1); glDepthMask(GL_TRUE); glClearDepth(1);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            // 检查场景恢复自己修改的深度、混合、剔除和写掩码状态。
            glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glEnable(GL_CULL_FACE); glDepthMask(GL_FALSE);
            scene->render({1.f,.016f,16.f/9.f});
            GLboolean mask=GL_TRUE; glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
            if (glIsEnabled(GL_DEPTH_TEST) || !glIsEnabled(GL_BLEND) || !glIsEnabled(GL_CULL_FACE) || mask)
                throw std::runtime_error("Asteroid belt did not restore render state");
            auto& image=instanced ? instancedPixels : ordinary;
            glReadPixels(0,0,320,180,GL_RGBA,GL_UNSIGNED_BYTE,image.data());
            if (amount==0 && instanced) { planetOnly=image; skyOnly=image; }
            if (glGetError()!=GL_NO_ERROR) throw std::runtime_error("Asteroid belt GL error");
        }
        size_t changed=0,mismatch=0;
        for (size_t p=0;p<ordinary.size();p+=4) {
            int difference=0,error=0;
            for (int c=0;c<3;++c) {
                difference+=std::abs(int(instancedPixels[p+c])-int(planetOnly[p+c]));
                error+=std::abs(int(instancedPixels[p+c])-int(ordinary[p+c]));
            }
            if (difference>12) ++changed;
            if (error>6) ++mismatch;
        }
        if ((amount==1 && changed==0) || (amount==2000 && changed<100) || mismatch>10)
            throw std::runtime_error("Asteroid belt instances missing or different from ordinary drawing");
        std::cout << "PASS asteroid belt instanced vs ordinary: " << amount << '\n';
    }
    // 星空背景：宇宙素材绝大部分是深色，不能用"非黑像素比例"判断，那样的阈值对
    // 《星际穿越》风格的黑洞全景毫无意义。改为与清屏色 (0,0,0) 对比平均亮度：
    // 没有天空盒时整屏接近 0；加载成功后视野内应有可见的星云与星点。
    // 同时统计最亮像素，确认确实存在亮星而不是一片死黑。
    size_t total=skyOnly.size()/4,litPixels=0,brightPixels=0;
    double sum=0.0;
    for (size_t p=0;p<skyOnly.size();p+=4) {
        const int value=std::max({skyOnly[p],skyOnly[p+1],skyOnly[p+2]});
        sum+=value;
        if (value>8) ++litPixels;
        if (value>80) ++brightPixels;
    }
    const double mean=sum/total;
    if (mean<3.0 || litPixels<total/5 || brightPixels<20)
        throw std::runtime_error("Asteroid belt skybox is missing or too dark");
    std::cout << "PASS asteroid belt skybox: mean " << mean << ", lit " << litPixels*100/total
              << "%, bright " << brightPixels << '\n';
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glViewport(0,0,width,height);

}
