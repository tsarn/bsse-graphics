add_library(glad "${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c")
add_library(glad::glad ALIAS glad)

target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
