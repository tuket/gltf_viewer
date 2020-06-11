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

template <typename T, typename... Ts>
static constexpr const T& min(const T& a, const Ts&... b) noexcept
{
    const T* r = &a;
    constexpr int n = sizeof...(b);
    const T* v[] = {(&b)...};
    for(int i = 0; i < n; i++)
        if(*v[i] < *r)
            r = v[i];
    return *r;
}

template <typename T, typename... Ts>
static constexpr const T& max(const T& a, const Ts&... b) noexcept
{
    const T* r = &a;
    constexpr int n = sizeof...(b);
    const T* v[] = {(&b)...};
    for(int i = 0; i < n; i++)
        if(*v[i] > *r)
            r = v[i];
    return *r;
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
