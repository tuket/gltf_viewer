project(imgui)

add_library(imgui STATIC
    imconfig.h
    imgui.h imgui.cpp
    imgui_internal.h
    imgui_draw.cpp
    imgui_widgets.cpp
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h
    imgui_impl_opengl3.h imgui_impl_opengl3.cpp
    imgui_impl_glfw.h imgui_impl_glfw.cpp
    imgui_demo.cpp
)

target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(imgui glfw glad glm)

add_executable(imgui_example imgui_example_glfw_opengl3.cpp)
target_link_libraries(imgui_example imgui)
