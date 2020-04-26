project(tg)

add_library(tg
	color.hpp
	img.hpp img.cpp
	geometry_utils.hpp geometry_utils.cpp
    texture_utils.hpp texture_utils.cpp
	shader_utils.hpp shader_utils.cpp
	cameras.hpp cameras.cpp
)

target_include_directories(tg PUBLIC  ${PROJECT_SOURCE_DIR}/..)

target_link_libraries(tg
	glm
	stb
	tl
	glad
	glfw
)

add_executable(tg_tests EXCLUDE_FROM_ALL
	tests/main.cpp
	tests/test_geometry.cpp
	tests/test_cylindermap_to_cubemap.cpp
	tests/test_cubemap.cpp
	tests/test_utils.hpp tests/test_utils.cpp
)
target_link_libraries(tg_tests
	glm
	tl
	tg
)
