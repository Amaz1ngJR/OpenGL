cmake_minimum_required(VERSION 3.22)
project(OpenGLLearnAssimp LANGUAGES C CXX)

if(MSVC)
    # 与 xmake 中的 MD/MDd 设置一致，不因静态库切换到 MT/MTd。
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
include(../AssimpOptions.cmake)
add_subdirectory(../../third_party/assimp assimp EXCLUDE_FROM_ALL)

# CMake 目标知道各平台的真实库名和配置后缀；对 xmake 暴露固定名称。
add_custom_target(stage_assimp ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:assimp>"
        "${CMAKE_BINARY_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}learn_assimp${CMAKE_STATIC_LIBRARY_SUFFIX}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:zlibstatic>"
        "${CMAKE_BINARY_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}learn_zlib${CMAKE_STATIC_LIBRARY_SUFFIX}"
    DEPENDS assimp zlibstatic
    VERBATIM
)
