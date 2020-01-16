project(stb)

add_library(stb STATIC stbi.h stb_image_write.h stb.c)

target_include_directories(stb PUBLIC ${PROJECT_SOURCE_DIR})