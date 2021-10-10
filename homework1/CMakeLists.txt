cmake_minimum_required(VERSION 3.12)
project(homework1 LANGUAGES C CXX)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/glad)
add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/glm)

add_executable(homework1 
    src/main.cpp
)

target_link_libraries(homework1 PRIVATE glad::glad glfw glm::glm)
set_property(TARGET homework1 PROPERTY CXX_STANDARD 20)
