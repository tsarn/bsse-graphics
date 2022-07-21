add_library(stb_image "${CMAKE_CURRENT_SOURCE_DIR}/stb_image.c")
add_library(stb_image::stb_image ALIAS stb_image)

target_include_directories(stb_image PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
