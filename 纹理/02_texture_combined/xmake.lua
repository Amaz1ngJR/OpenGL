add_rules("mode.debug", "mode.release")
set_project("TextureCombined")
set_languages("cxx17")

local root = path.join(os.scriptdir(), "../..")

target("glfw_local")
    set_kind("static")
    set_group("third_party")
    set_warnings("none")
    add_includedirs(path.join(root, "third_party/glfw-3.3.8/include"))
    if is_plat("macosx") then
        add_files(path.join(root, "third_party/glfw-3.3.8/src/*.m"), {mflags = "-fno-objc-arc"})
        add_files(path.join(root, "third_party/glfw-3.3.8/src/*.c"))
        add_files(path.join(root, "third_party/glfw-3.3.8/src/posix_thread.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/win32_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/wgl_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/wl_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/x11_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/posix_time.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/null_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/linux_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/xkb_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/glx_*.c"))
        add_mxflags("-fno-objc-arc")
        add_frameworks("Cocoa", "CoreVideo", "IOKit")
        add_defines("_GLFW_COCOA")
    elseif is_plat("windows") then
        add_files(
            path.join(root, "third_party/glfw-3.3.8/src/context.c"),
            path.join(root, "third_party/glfw-3.3.8/src/init.c"),
            path.join(root, "third_party/glfw-3.3.8/src/input.c"),
            path.join(root, "third_party/glfw-3.3.8/src/monitor.c"),
            path.join(root, "third_party/glfw-3.3.8/src/vulkan.c"),
            path.join(root, "third_party/glfw-3.3.8/src/window.c"),
            path.join(root, "third_party/glfw-3.3.8/src/egl_context.c"),
            path.join(root, "third_party/glfw-3.3.8/src/osmesa_context.c"),
            path.join(root, "third_party/glfw-3.3.8/src/wgl_context.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_init.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_monitor.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_window.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_module.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_joystick.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_time.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_thread.c"),
            path.join(root, "third_party/glfw-3.3.8/src/win32_unicode.c")
        )
        add_defines("_GLFW_WIN32")
        add_syslinks("gdi32", "shell32")
    end

target("glad_local")
    set_kind("static")
    set_group("third_party")
    add_includedirs(path.join(root, "third_party/glad/include"))
    add_files(path.join(root, "third_party/glad/src/glad.c"))

target("TextureCombined")
    set_kind("binary")
    add_files("src/*.cpp")
    add_deps("glfw_local", "glad_local")
    add_includedirs("include")
    add_includedirs(path.join(root, "third_party/glfw-3.3.8/include"))
    add_includedirs(path.join(root, "third_party/glad/include"))
    add_includedirs(path.join(root, "third_party/glm"))
    set_targetdir(path.join(os.scriptdir(), "build"))
    set_rundir(path.join(os.scriptdir(), "build"))
    if is_plat("macosx") then
        add_frameworks("OpenGL")
    elseif is_plat("windows") then
        add_syslinks("opengl32", "gdi32")
    end
