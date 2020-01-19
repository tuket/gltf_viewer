#pragma once

#include <tl/int_types.hpp>
#include <tl/move.hpp>
#include <initializer_list>
#include <assert.h>

namespace tl
{
// Fixed Vector
template <typename T, size_t N>
class FVector
{
public:
    constexpr FVector() : _size(0) {}
    constexpr FVector(std::initializer_list<T> l);
    
    T& operator[](size_t i);
    const T& operator[](size_t i)const;

    T* begin() { return _data; }
    const T* begin()const { return _data; }
    T* end() { return _data + _size; }
    const T* end()const { return _data + _size; }

    size_t size()const { return _size; }
    constexpr size_t capacity()const { return N; }

    void resize(size_t n);
    void push_back(T&& x);
    void pop_back();

private:
    template<typename... Args>
    void _init(size_t i, Args&&... args);

    void _destroy(size_t i);

    char _data[sizeof(T)*N];
    size_t _size;
};

template <typename T, size_t N>
constexpr FVector<T, N>::FVector(std::initializer_list<T> l)
{
    static_assert(l.size() <= N, "too many elements in initialization of FVector");
    _size = l.size();
    for(size_t i = 0; i < N; i++)
        _init(i, l[i]);
}

template <typename T, size_t N>
T& FVector<T, N>::operator[](size_t i) {
    assert(i < _size);
    return begin()[i];
}
template <typename T, size_t N>
const T& FVector<T, N>::operator[](size_t i)const {
    assert(i < _size);
    return begin()[i];
}

template <typename T, size_t N>
void FVector<T, N>::resize(size_t n) {
    assert(n <= N);
    for(size_t i = _size; i < n; i++)
        _init(i);
    for(size_t i = n; i < _size; i++)
        _destroy(i);
    _size = n;
}

template <typename T, size_t N>
void FVector<T, N>::push_back(T&& x) {
    assert(_size < N);
    _init(_size, tl::forward<T>(x));
    _size++;
}

template <typename T, size_t N>
void FVector<T, N>::pop_back() {
    asert(_size > 0);
    _destroy(_size);
    _size--;
}

template <typename T, size_t N>
template<typename... Args>
void FVector<T, N>::_init(size_t i, Args&&... args) {
    new ((void*)(begin()+i)) T(tl::forward<Args>(args)...);
}

template <typename T, size_t N>
void FVector<T, N>::_destroy(size_t i) {
    begin()[i].~T();
}

}
