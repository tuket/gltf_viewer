project(cgltf)

add_library(cgltf STATIC
    cgltf.h
    cgltf_write.h
    cgltf.c
    cgltf_write.c
)
target_include_directories(cgltf PUBLIC ${PROJECT_SOURCE_DIR})
