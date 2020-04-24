#include "shader_utils.hpp"

#include <glad/glad.h>

namespace tg
{

char* getShaderCompileErrors(u32 shader, tl::Span<char> buffer)
{
    i32 ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(ok) {
        glGetShaderInfoLog(shader, buffer.size(), nullptr, buffer.begin());
        return buffer.begin();
    }
    return nullptr;
}

char* getShaderLinkErrors(u32 prog, tl::Span<char> buffer)
{
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(prog, buffer.size(), nullptr, buffer.begin());
        return buffer.begin();
    }
    return nullptr;
}


}
