#pragma once

#include "int_types.hpp"

namespace tl
{

template <typename T>
class CArray
{
public:
    CArray();
    CArray(const T* data, size_t size);
    CArray(const T* from, const T* to);
    CArray(const CArray& o);

    const T& operator[](size_t i)const;

    const T* begin()const;
    const T* end()const;

    const T* data()const;
    size_t size()const;

    CArray subArray(size_t from, size_t to)const; // to is not included i.e: [from, to)

private:
    const T* _data;
    size_t _size;
};


// ---------------------------------------------------------------------------------------------
template <typename T>
CArray<T>::CArray()
    : _data(nullptr)
    , _size(0)
{}

template <typename T>
CArray<T>::CArray(const T* data, size_t size)
    : _data(data)
    , _size(size)
{}

template <typename T>
CArray<T>::CArray(const T* from, const T* to)
    : _data(from)
    , _size(to - from)
{}

template <typename T>
CArray<T>::CArray(const CArray& o)
    : _data(o._data)
    , _size(o._size)
{}

template <typename T>
const T& CArray<T>::operator[](size_t i)const {
    assert(i < _size);
    return _data[i];
}

template <typename T>
const T* CArray<T>::begin()const {
    return _data;
}

template <typename T>
const T* CArray<T>::end()const {
    return _data + _size;
}

template <typename T>
const T* CArray<T>::data()const {
    return _data;
}

template <typename T>
size_t CArray<T>::size()const {
    return _size;
}

template <typename T>
CArray<T> CArray<T>::subArray(size_t from, size_t to)const {
    assert(from <= to && to <= _size);
    CArray a(_data + from, _data + to);
}

}
