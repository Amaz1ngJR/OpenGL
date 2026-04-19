cmake_minimum_required(VERSION 3.10)
project(TexturesBase)
set(CMAKE_CXX_STANDARD 17)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(THIRD_PARTY_DIR ${ROOT_DIR}/third_party)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/glfw-3.3.8 ${CMAKE_CURRENT_BINARY_DIR}/glfw)

# GLAD
add_library(glad STATIC ${THIRD_PARTY_DIR}/glad/src/glad.c)
target_include_directories(glad PUBLIC ${THIRD_PARTY_DIR}/glad/include)

# Main
file(GLOB SOURCES src/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${THIRD_PARTY_DIR}/glfw-3.3.8/include
    ${THIRD_PARTY_DIR}/glad/include
    ${THIRD_PARTY_DIR}/glm
)
target_link_libraries(${PROJECT_NAME} glfw glad)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
elseif(WIN32)
    target_link_libraries(${PROJECT_NAME} opengl32 gdi32)
endif()

# Set working directory to project root for shader loading
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
