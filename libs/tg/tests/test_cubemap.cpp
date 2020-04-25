#include <stdio.h>
#include <assert.h>
#define GLM_FORCE_RADIANS
#define GLAD_DEBUG
#include <glad/glad.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <tg/shader_utils.hpp>
#include <tg/cameras.hpp>
#include <tl/defer.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <stbi.h>

constexpr float PI = glm::pi<float>();

static GLFWwindow* window;

static const char s_vertShadSrc[] =
R"GLSL(
#version 330 core

in layout(location = 0) vec3 a_pos;

out vec3 v_modelPos;

uniform mat4 u_modelViewProjMat;


void main()
{
    v_modelPos = a_pos;
    gl_Position = u_modelViewProjMat * vec4(a_pos, 1);
}
)GLSL";

static const char s_fragShadSrc[] =
R"GLSL(
#version 330 core

layout(location = 0) out vec4 o_color;

in vec3 v_modelPos;

uniform samplerCube u_cubemap;

void main()
{
    o_color = texture(u_cubemap, normalize(v_modelPos));
}
)GLSL";

static float s_cubeVerts[6*6*(3+2)] = {
    // x, y, z,  u,     v
    // LEFT
    -1, -1, -1,
    -1, -1, +1,
    -1, +1, +1,
    -1, -1, -1,
    -1, +1, +1,
    -1, +1, -1,
    // RIGHT
    +1, -1, +1,
    +1, -1, -1,
    +1, +1, -1,
    +1, -1, +1,
    +1, +1, -1,
    +1, +1, +1,
    // BOTTOM
    -1, -1, -1,
    +1, -1, -1,
    +1, -1, +1,
    -1, -1, -1,
    +1, -1, +1,
    -1, -1, +1,
    // TOP
    -1, +1, +1,
    +1, +1, +1,
    +1, +1, -1,
    -1, +1, +1,
    +1, +1, -1,
    -1, +1, -1,
    // FRONT
    -1, -1, +1,
    +1, -1, +1,
    +1, +1, +1,
    -1, -1, +1,
    +1, +1, +1,
    -1, +1, +1,
    // BACK
    +1, -1, -1,
    -1, -1, -1,
    -1, +1, -1,
    +1, -1, -1,
    -1, +1, -1,
    +1, +1, -1,
};

struct UnifLocs {
    i32 modelViewProj;
    i32 cubemap;
} unifLocs;

static char s_buffer[4*1024];

static struct OrbitCameraInfo {
    float heading, pitch;
    float distance;
} orbitCam;

static bool mousePressed = false;
static double prevMouseX, prevMouseY;

void onMouseClick(GLFWwindow* window, int button, int action, int mods)
{
    if(button == GLFW_MOUSE_BUTTON_1)
        mousePressed = action == GLFW_PRESS;
}

void onMouseMove(GLFWwindow* window, double x, double y)
{
    if(mousePressed) {
        const float dx = (float)x - prevMouseX;
        const float dy = (float)y - prevMouseY;
        constexpr float speed = PI;
        int windowW, windowH;
        glfwGetWindowSize(window, &windowW, &windowH);
        orbitCam.heading += speed * dx / windowW;
        while(orbitCam.heading < 0)
            orbitCam.heading += 2*PI;
        while(orbitCam.heading > 2*PI)
            orbitCam.heading -= 2*PI;
        orbitCam.pitch += speed * dy / windowH;
        orbitCam.pitch = glm::clamp(orbitCam.pitch, -0.45f*PI, +0.45f*PI);
    }
    prevMouseX = (float)x;
    prevMouseY = (float)y;
}

void onMouseWheel(GLFWwindow* window, double dx, double dy)
{
    const float speed = 1.04f;
    orbitCam.distance *= pow(speed, (float)dy);
    orbitCam.distance = glm::max(orbitCam.distance, 0.01f);
}

void glErrorCallback(const char *name, void *funcptr, int len_args, ...) {
    GLenum error_code;
    error_code = glad_glGetError();
    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %s in %s\n", gluErrorString(error_code), name);
        assert(false);
    }
}

bool test_cubemap()
{
    orbitCam.heading = orbitCam.pitch = 0;
    orbitCam.distance = 4;

    glfwSetErrorCallback(+[](int error, const char* description) {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    });
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "test", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Enable vsync

    if (gladLoadGL() == 0) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }
    glad_set_post_callback(glErrorCallback);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glfwSetMouseButtonCallback(window, onMouseClick);
    glfwSetCursorPosCallback(window, onMouseMove);
    glfwSetScrollCallback(window, onMouseWheel);

    u32 cubemapTexture;
    glGenTextures(1, &cubemapTexture);
    defer(glDeleteTextures(1, &cubemapTexture));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    {
        int w, h, nc;
        u8* data = stbi_load("cubemap_test_rgb.png", &w, &h, &nc, 3);
        if(data)
        {
            defer(stbi_image_free(data));
            const int side = w / 4;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
            defer(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
            auto upload = [&](GLenum face, int offset) {
                glTexImage2D(face, 0, GL_RGB8, side, side, 0, GL_RGB, GL_UNSIGNED_BYTE, data + offset);
            };
            upload(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 3*w*side);
            upload(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 3*(w*side + 2*side));
            upload(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 3*(w*2*side + side));
            upload(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 3*side);
            upload(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 3*(w*side + 3*side));
            upload(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 3*(w*side + side));
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
        else {
            printf("error loading img\n");
            return 1;
        }
    }

    u32 prog = glCreateProgram();
    defer(glDeleteProgram(prog));
    {
        u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
        defer(glDeleteShader(vertShad));
        const char* vertShadSrc = s_vertShadSrc;
        glShaderSource(vertShad, 1, &vertShadSrc, nullptr);
        glCompileShader(vertShad);
        if(const char* errorMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
            printf("Error compiling vertex shader:\n%s\n", errorMsg);
            return 0;
        }

        u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        defer(glDeleteShader(fragShad));
        const char* fragShadSrc = s_fragShadSrc;
        glShaderSource(fragShad, 1, &fragShadSrc, nullptr);
        glCompileShader(fragShad);
        if(const char* errorMsg = tg::getShaderCompileErrors(fragShad, s_buffer)) {
            printf("Error compiling fragment shader:\n%s\n", errorMsg);
            return 0;
        }

        glAttachShader(prog, vertShad);
        glAttachShader(prog, fragShad);
        glLinkProgram(prog);
        if(const char* errorMsg = tg::getShaderLinkErrors(prog, s_buffer)) {
            printf("Error linking:\n%s\n", errorMsg);
            return 0;
        }
    }

    glUseProgram(prog);
    unifLocs.modelViewProj = glGetUniformLocation(prog, "u_modelViewProjMat");
    unifLocs.cubemap = glGetUniformLocation(prog, "u_cubemap");
    glUniform1i(unifLocs.cubemap, 0);

    u32 vao, vbo;
    glGenVertexArrays(1, &vao);
    defer(glDeleteVertexArrays(1, &vao));
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    defer(glDeleteVertexArrays(1, &vbo));
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_cubeVerts), s_cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // draw scene
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glScissor(0, 0, w, h);

        glClearColor(0, 0.2f, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 modelMtx(1);
        const glm::mat4 viewMtx = tg::calcOrbitCameraMtx(orbitCam.heading, orbitCam.pitch, orbitCam.distance);
        const glm::mat4 projMtx = glm::perspective(glm::radians(45.f), float(w)/h, 0.1f, 1000.f);
        //const glm::mat4 projMtx(1);
        const glm::mat4 modelViewProj = projMtx * glm::mat4(viewMtx) * modelMtx;
        //const glm::mat4 modelViewProj(1);
        glUniformMatrix4fv(unifLocs.modelViewProj, 1, GL_FALSE, &modelViewProj[0][0]);

        glDrawArrays(GL_TRIANGLES, 0, 6*6);

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    return true;
}
