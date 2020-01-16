#pragma once

#include "int_types.hpp"

namespace tl
{

// computes the next power of two, unless it's already a power of two
static constexpr u32 nextPowerOf2(u32 x) noexcept
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

// computes the next power of two, unless it's already a power of two
static constexpr u64 nextPowerOf2(u64 x) noexcept
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;
    return x;
}

}
