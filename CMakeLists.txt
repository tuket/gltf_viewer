
cmake_minimum_required(VERSION 3.8)
project(gltf_viewer)

set (CMAKE_CXX_STANDARD 17)

# this function preppends a path to all files in a list
FUNCTION(PREPEND var prefix)
SET(listVar "")
   FOREACH(f ${ARGN})
   LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
	include_directories(${OPENGL_INCLUDE_DIRS})
	link_libraries(${OPENGL_LIBRARIES})
endif()

add_subdirectory(libs/glad)
add_subdirectory(libs/glfw)
add_subdirectory(libs/imgui)
add_subdirectory(libs/stb)
add_subdirectory(libs/glm)
add_subdirectory(libs/cgltf)
add_subdirectory(libs/tl)
add_subdirectory(libs/tg)

set(SOURCES
    main.cpp
    scene.hpp scene.cpp
    utils.hpp utils.cpp
	shaders.hpp shaders.cpp
)
PREPEND(SOURCES "src/" ${SOURCES})

add_executable(gltf_viewer
    ${SOURCES}
)
target_link_libraries(gltf_viewer glad)
target_link_libraries(gltf_viewer glfw)
target_link_libraries(gltf_viewer imgui)
target_link_libraries(gltf_viewer stb)
target_link_libraries(gltf_viewer glm)
target_link_libraries(gltf_viewer cgltf)
target_link_libraries(gltf_viewer tl)
target_link_libraries(gltf_viewer tg)

add_library(textool
	textool/textool.cpp
)
target_link_libraries(textool glfw)
target_link_libraries(textool stb)
target_link_libraries(textool glm)
target_link_libraries(textool tl)
target_link_libraries(textool tg)

