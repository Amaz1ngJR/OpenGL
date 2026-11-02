set_project("OpenGLLearn")
set_languages("c99", "cxx17")
add_rules("mode.debug", "mode.release")

-- 独立于 CMake，直接编译仓库中的依赖，不从网络下载包。
local root = os.scriptdir()
local glfw = path.join(root, "third_party/glfw-3.3.8")
local imgui = path.join(root, "third_party/imgui")

target("glfw_local")
    set_kind("static")
    set_default(false)
    set_group("third_party")
    set_warnings("none")
    add_includedirs(path.join(glfw, "include"), {public = true})
    add_files(path.join(glfw, "src/context.c"), path.join(glfw, "src/init.c"),
              path.join(glfw, "src/input.c"), path.join(glfw, "src/monitor.c"),
              path.join(glfw, "src/vulkan.c"), path.join(glfw, "src/window.c"),
              path.join(glfw, "src/egl_context.c"), path.join(glfw, "src/osmesa_context.c"))
    if is_plat("macosx") then
        add_defines("_GLFW_COCOA")
        add_files(path.join(glfw, "src/cocoa_*.m"), path.join(glfw, "src/nsgl_context.m"),
                  path.join(glfw, "src/cocoa_time.c"), path.join(glfw, "src/posix_thread.c"))
        add_mflags("-fno-objc-arc")
        add_frameworks("Cocoa", "IOKit", "CoreFoundation", {public = true})
    elseif is_plat("windows") then
        add_defines("_GLFW_WIN32", "UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS")
        add_files(path.join(glfw, "src/win32_*.c"), path.join(glfw, "src/wgl_context.c"))
        add_syslinks("gdi32", "user32", "shell32", {public = true})
    end
    on_load(function (target)
        if not target:is_plat("macosx", "windows") then
            raise("The root xmake build currently supports macOS and Windows only.")
        end
    end)

target("glad_local")
    set_kind("static")
    set_default(false)
    set_group("third_party")
    add_includedirs("third_party/glad/include", {public = true})
    add_files("third_party/glad/src/glad.c")

target("OpenGLLearn")
    set_kind("binary")
    set_default(true)
    add_deps("glfw_local", "glad_local")
    add_files("engine/src/main.cpp", "engine/src/Shader.cpp", "engine/src/Camera.cpp", "engine/src/Scenes.cpp")
    add_files(path.join(imgui, "imgui.cpp"), path.join(imgui, "imgui_draw.cpp"),
              path.join(imgui, "imgui_tables.cpp"), path.join(imgui, "imgui_widgets.cpp"),
              path.join(imgui, "backends/imgui_impl_glfw.cpp"),
              path.join(imgui, "backends/imgui_impl_opengl3.cpp"))
    add_includedirs("engine/include", "third_party", "third_party/glm",
                    "third_party/imgui", "third_party/imgui/backends")
    add_defines('OPENGL_LEARN_ROOT="' .. root:gsub("\\", "/") .. '"')
    if is_plat("macosx") then
        add_frameworks("OpenGL")
    elseif is_plat("windows") then
        add_syslinks("opengl32")
        add_cxxflags("/utf-8", {tools = "cl"})
    end
    -- 两个构建系统的可执行文件不互相覆盖。
    set_targetdir("build/xmake/$(plat)/$(arch)/$(mode)")
    set_rundir(root)
