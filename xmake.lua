set_project("OpenGLLearn")
set_languages("c99", "cxx17")
add_rules("mode.debug", "mode.release")

-- 第三方依赖均来自仓库；Assimp 静态库用与主程序相同的 CMake/编译器配置构建。
local root = os.scriptdir()
local third_party = path.join(root, "third_party")
local glfw = path.join(third_party, "glfw-3.3.8")
local imgui = path.join(third_party, "imgui")

-- GLFW 静态库：只编译当前平台的窗口与 OpenGL 上下文后端。
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
        if is_mode("debug") then set_runtimes("MDd") else set_runtimes("MD") end
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
    add_includedirs(path.join(third_party, "glad/include"), {public = true})
    add_files(path.join(third_party, "glad/src/glad.c"))
    if is_plat("windows") then
        if is_mode("debug") then set_runtimes("MDd") else set_runtimes("MD") end
    end

-- 使用 Assimp 官方 CMake 工程编译，避免在 xmake 中维护数百个源码和导入器清单。
target("assimp_build")
    set_kind("phony")
    set_default(false)
    set_group("third_party")
    on_load(function (target)
        import("core.project.config")
        local mode = config.mode() or "release"
        local build_dir = path.join(os.projectdir(), "build/xmake-assimp", target:plat(), target:arch(), mode)
        target:data_set("assimp_build_dir", build_dir)
        target:add("includedirs", path.join(os.projectdir(), "third_party/assimp/include"),
                   path.join(build_dir, "assimp/include"), {public = true})
        target:add("linkdirs", path.join(build_dir, "lib"), {public = true})
        target:add("links", "learn_assimp", "learn_zlib", {public = true})
    end)
    on_build(function (target)
        import("core.project.config")
        import("lib.detect.find_tool")
        local cmake = assert(find_tool("cmake"), "Assimp requires CMake >= 3.22 in PATH")
        local mode = config.mode() == "debug" and "Debug" or "Release"
        local build_dir = target:data("assimp_build_dir")
        local args = {"-S", path.join(os.projectdir(), "cmake/AssimpForXmake"), "-B", build_dir,
                      "-DCMAKE_BUILD_TYPE=" .. mode}
        if target:is_plat("windows") then
            -- 不混用 MinGW ABI；用 VS 环境中检测到的版本，而不是写死当前默认版本。
            local msvc = target:toolchain("msvc")
            assert(msvc, "Windows Assimp build requires the MSVC toolchain")
            msvc:check()
            local vs = tostring(config.get("vs") or msvc:config("vs") or "2022")
            local generators = {["2022"] = "Visual Studio 17 2022", ["2019"] = "Visual Studio 16 2019"}
            local arches = {x64 = "x64", x86 = "Win32", arm64 = "ARM64"}
            table.join2(args, {"-G", assert(generators[vs], "Use Visual Studio 2019 or 2022"),
                              "-A", assert(arches[target:arch()], "Unsupported MSVC architecture")})
            if config.get("vs_toolset") then table.join2(args, {"-T", config.get("vs_toolset")}) end
        elseif target:is_plat("macosx") then
            local arches = {arm64 = "arm64", x86_64 = "x86_64"}
            table.insert(args, "-DCMAKE_OSX_ARCHITECTURES=" .. assert(arches[target:arch()]))
        end
        os.vrunv(cmake.program, args)
        os.vrunv(cmake.program, {"--build", build_dir, "--config", mode, "--parallel", "4"})
    end)

target("OpenGLLearn")
    set_kind("binary")
    set_default(true)
    add_deps("glfw_local", "glad_local", "assimp_build")
    add_files("engine/src/main.cpp", "engine/src/Shader.cpp", "engine/src/Camera.cpp", "engine/src/Scenes.cpp",
              "engine/src/ModelScene.cpp", "engine/src/FileDialog.cpp")
    add_files(path.join(imgui, "imgui.cpp"), path.join(imgui, "imgui_draw.cpp"),
              path.join(imgui, "imgui_tables.cpp"), path.join(imgui, "imgui_widgets.cpp"),
              path.join(imgui, "backends/imgui_impl_glfw.cpp"),
              path.join(imgui, "backends/imgui_impl_opengl3.cpp"))
    add_includedirs("engine/include", "third_party", "third_party/glm",
                    "third_party/imgui", "third_party/imgui/backends")
    add_defines('OPENGL_LEARN_ROOT="' .. root:gsub("\\", "/") .. '"')
    if is_plat("macosx") then
        add_files("engine/src/FileDialogMac.mm")
        add_frameworks("OpenGL", "AppKit")
    elseif is_plat("windows") then
        -- 静态 Assimp 与主程序使用相同的动态 CRT，避免 LNK2038。
        if is_mode("debug") then set_runtimes("MDd") else set_runtimes("MD") end
        add_syslinks("opengl32", "gdi32", "comdlg32")
        add_cxxflags("/utf-8", {tools = "cl"})
    end
    set_targetdir("build/xmake/$(plat)/$(arch)/$(mode)")
    set_rundir(root)
