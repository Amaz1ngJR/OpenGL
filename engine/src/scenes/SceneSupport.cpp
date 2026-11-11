// stb_image 是单头文件库：宏会启用函数实现，因此只能在一个翻译单元中定义。
#define STB_IMAGE_IMPLEMENTATION
#include "SceneSupport.h"

namespace sceneSupport {
std::unique_ptr<Shader> loadShader(const std::string& root, const std::string& directory,
                                  const std::string& name) {
    // root 来自构建时写入的仓库路径，而非进程当前目录；着色器加载不依赖启动位置。
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
    // 每面两个三角形共六个顶点，绕序从立方体外部看为逆时针，配合 GL_CCW 正面定义。
    const int order[] = {0,1,2,0,2,3};
    std::vector<float> vertices;
    for (const auto& face : corners) {
        // 两条边的叉积给出面法线，normalize 只保留方向。不同面不能共用平滑法线。
        const glm::vec3 normal = glm::normalize(glm::cross(face[1] - face[0], face[2] - face[0]));
        for (int index : order) {
            const auto& p = face[index];
            vertices.insert(vertices.end(), {p.x, p.y, p.z});
            // 输出两种交错布局：位置+法线供光照，或位置+UV 供贴图演示。
            if (withNormals) vertices.insert(vertices.end(), {normal.x, normal.y, normal.z});
            else vertices.insert(vertices.end(), {uv[index].x, uv[index].y});
        }
    }
    return vertices;
}

} // namespace sceneSupport
