cmake_minimum_required(VERSION 3.12)
project(homework2 LANGUAGES C CXX)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/glad)
add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/stb_image)

add_executable(homework2 
    src/main.cpp
)

target_link_libraries(homework2 PRIVATE glad::glad glfw glm::glm stb_image::stb_image)
set_property(TARGET homework2 PROPERTY CXX_STANDARD 20)
