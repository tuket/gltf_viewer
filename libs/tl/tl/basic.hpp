#pragma once

#include "int_types.hpp"
#include "move.hpp"

namespace tl
{

typedef decltype(nullptr) nullptr_t;

template <typename T, size_t N>
constexpr size_t size(T(&)[N]) { return N; }

template<typename T>
void swap(T& a, T& b)
{
    T x = move(a);
    a = move(b);
    b = move(x);
}

template <typename T>
static constexpr T min(const T& a, const T& b) noexcept
{
    // return a if equal
    return (a > b) ? b : a;
}

template <typename T>
static constexpr T max(const T& a, const T& b) noexcept
{
    // return a if equal
    return (b > a) ? b : a;
}

template <typename T>
static constexpr void minMax(T& a, T& b) noexcept
{
    if(a > b)
        tl::swap(a, b);
}

template <typename T>
static constexpr T clamp(const T& x, const T& min, const T& max) noexcept
{
    return (x > min) ? (x < max ? x : max) : min;
}

}
