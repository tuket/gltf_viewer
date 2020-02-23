#pragma once

#include <tl/int_types.hpp>
#include <assert.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace tg
{

struct Color3u8
{
    u8 r, g, b;

    u8& operator[](int i);
    const u8& operator[](int i)const;

    operator glm::vec3()const;
};

struct Color4u8
{
    u8 r, g, b, a;

    u8& operator[](int i);
    const u8& operator[](int i)const;

    operator glm::vec4()const;
};

// --- impl -------------------------------------------------------------------------------------------------------

u8& Color3u8::operator[](int i) {
    assert(i >= 0 && i < 3);
    return (&r)[i];
}

const u8& Color3u8::operator[](int i)const {
    assert(i >= 0 && i < 3);
    return (&r)[i];
}

Color3u8::operator glm::vec3()const {
    return { r / 255.f, g / 255.f, b / 255.f };
}

u8& Color4u8::operator[](int i) {
    assert(i >= 0 && i < 4);
    return (&r)[i];
}

const u8& Color4u8::operator[](int i)const {
    assert(i >= 0 && i < 4);
    return (&r)[i];
}

Color4u8::operator glm::vec4()const {
    return { r / 255.f, g / 255.f, b / 255.f, a / 255.f };
}

}
