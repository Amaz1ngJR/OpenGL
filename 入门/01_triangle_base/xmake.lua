-- ============================================================================
-- xmake.lua —— xmake 构建脚本（CMake 之外的另一套构建方式）
-- ----------------------------------------------------------------------------
-- 与 CMakeLists.txt 等价，二选一即可。xmake 的优势是配置语法更简洁、
-- 跨平台默认行为更友好。常用命令：
--     xmake f -m debug      -- 配置为 Debug 模式
--     xmake                  -- 编译
--     xmake run TriangleBase -- 运行
-- ============================================================================

-- 启用 debug / release 两种构建模式（决定优化等级、调试符号等）
add_rules("mode.debug", "mode.release")
set_project("TriangleBase")     -- 项目名
set_languages("cxx17")          -- 使用 C++17

-- 第三方库的根路径（仓库根目录的 third_party）
local root = path.join(os.scriptdir(), "../..")

-- ----------------------------------------------------------------------------
-- target: glfw_local —— 把 GLFW 源码编译为本地静态库
-- 不同平台使用不同的源文件子集（避免引入其他平台的实现导致编译错误）
-- ----------------------------------------------------------------------------
target("glfw_local")
    set_kind("static")              -- 静态库
    set_group("third_party")        -- IDE 中归类到 third_party 分组
    set_warnings("none")            -- 关闭第三方代码的警告以减少干扰
    add_includedirs(path.join(root, "third_party/glfw-3.3.8/include"))

    if is_plat("macosx") then
        -- macOS：包含所有 .m / .c，再剔除其他平台的实现
        add_files(path.join(root, "third_party/glfw-3.3.8/src/*.m"), {mflags = "-fno-objc-arc"})
        add_files(path.join(root, "third_party/glfw-3.3.8/src/*.c"))
        add_files(path.join(root, "third_party/glfw-3.3.8/src/posix_thread.c"))
        -- 移除非 macOS 平台才使用的源文件
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/win32_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/wgl_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/wl_*.c"))    -- Wayland
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/x11_*.c"))   -- X11
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/posix_time.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/null_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/linux_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/xkb_*.c"))
        remove_files(path.join(root, "third_party/glfw-3.3.8/src/glx_*.c"))   -- GLX (X11 OpenGL)

        add_mxflags("-fno-objc-arc")                                    -- ObjC 文件不使用 ARC
        add_frameworks("Cocoa", "CoreVideo", "IOKit")                   -- 链接系统 Framework
        add_defines("_GLFW_COCOA")                                      -- 选择 Cocoa 后端

    elseif is_plat("windows") then
        -- Windows：显式列出所需源文件
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
        add_defines("_GLFW_WIN32")                  -- 选择 Win32 后端
        add_syslinks("gdi32", "shell32")            -- 链接 Windows 系统库
    end

-- ----------------------------------------------------------------------------
-- target: glad_local —— 把 GLAD 编译为静态库
-- ----------------------------------------------------------------------------
target("glad_local")
    set_kind("static")
    set_group("third_party")
    add_includedirs(path.join(root, "third_party/glad/include"))
    add_files(path.join(root, "third_party/glad/src/glad.c"))

-- ----------------------------------------------------------------------------
-- target: TriangleBase —— 主可执行程序
-- ----------------------------------------------------------------------------
target("TriangleBase")
    set_kind("binary")                              -- 可执行文件
    add_files("src/*.cpp")                          -- 收集 src 下所有 .cpp
    add_deps("glfw_local", "glad_local")            -- 依赖前面两个本地静态库

    -- 头文件搜索路径
    add_includedirs("include")                                          -- 项目头文件
    add_includedirs(path.join(root, "third_party/glfw-3.3.8/include"))  -- GLFW
    add_includedirs(path.join(root, "third_party/glad/include"))        -- GLAD
    add_includedirs(path.join(root, "third_party/glm"))                 -- GLM (header-only)

    -- 输出与运行目录都设为项目下的 build/，方便管理产物
    set_targetdir(path.join(os.scriptdir(), "build"))
    set_rundir(path.join(os.scriptdir(), "build"))

    -- 按平台链接 OpenGL
    if is_plat("macosx") then
        add_frameworks("OpenGL")                    -- macOS 系统 Framework
    elseif is_plat("windows") then
        add_syslinks("opengl32", "gdi32")           -- Windows 系统库
    end
