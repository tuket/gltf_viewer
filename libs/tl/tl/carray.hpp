#pragma once

#include "int_types.hpp"

namespace tl
{

template <typename T>
class Array
{
public:
    Array();
    Array(T* data, size_t size);
    Array(T* from, T* to);
    template <size_t N>
    Array(T (&data)[N]);
    Array(Array& o);
    Array(const Array& o);
    operator T*() { return _data; }
    operator const T*()const { return _data; }

    T& operator[](size_t i);
    const T& operator[](size_t i)const;

    T* begin() { return _data; }
    T* end() { return _data + _size; }
    const T* begin()const { return _data; }
    const T* end()const { return _data + _size;}
    size_t size()const { return _size; }

    Array<T> subArray(size_t from, size_t to);
    const Array<T> subArray(size_t from, size_t to)const; // to is not included i.e: [from, to)

private:
    T* _data;
    size_t _size;
};

// ---------------------------------------------------------------------------------------------
template <typename T>
Array<T>::Array()
    : _data(nullptr)
    , _size(0)
{}

template <typename T>
Array<T>::Array(T* data, size_t size)
    : _data(data)
    , _size(size)
{}

template <typename T>
Array<T>::Array(T* from, T* to)
    : _data(from)
    , _size(to - from)
{}

template <typename T>
template <size_t N>
Array<T>::Array(T (&data)[N])
    : _data(&data[0])
    , _size(N)
{}

template <typename T>
Array<T>::Array(Array<T>& o)
    : _data(o._data)
    , _size(o._size)
{}

template <typename T>
Array<T>::Array(const Array<T>& o)
    : _data(o._data)
    , _size(o._size)
{}

template <typename T>
T& Array<T>::operator[](size_t i) {
    assert(i < _size);
    return _data[i];
}

template <typename T>
const T& Array<T>::operator[](size_t i)const {
    assert(i < _size);
    return _data[i];
}

template <typename T>
Array<T> Array<T>::subArray(size_t from, size_t to) {
    assert(from <= to && to <= _size);
    return Array<T>(_data + from, _data + to);
}

template <typename T>
const Array<T> Array<T>::subArray(size_t from, size_t to)const {
    assert(from <= to && to <= _size);
    return Array<T>(_data + from, _data + to);
}

template <typename T>
using CArray = Array<const T>;

}
