#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include "color.hpp"

namespace tg
{

template <typename T>
class ImgView
{
public:
    ImgView() : _w(0), _h(0), _stride(0), _data(nullptr) {}
    ImgView(int w, int h, T* data) : _w(h), _h(h), _stride(w), _data(data) {}
    ImgView(int w, int h, int stride, T* data) : _w(w), _h(h), _stride(stride), _data(data) {}

    const T& operator()(int x, int y)const;
    T& operator()(int x, int y);

    int width()const { return _w; }
    int height()const { return _h; }
    int stride()const { return _stride; }
    int strideInBytes()const { return _stride * sizeof(T); }

    ImgView subImg(int x, int y, int w, int h);
    ImgView<const T> subImb(int x, int y, int w, int h)const;

protected:
    int _w, _h;
    int _stride; // this stride is in elements, not in bytes. Stride is the number of elements from one row to next one
    T* _data;
};

template <typename T>
class Img : public ImgView<T>
{
public:
    Img() : ImgView<T>() {}
    Img(int w, int h) : ImgView<T>(w, h, new T[w*h]) {}
    ~Img() { delete[] ImgView<T>::_data; }
};

template <typename T>
struct CubeImg {
    Img<T> left, right, down, up, front, back;
};

template <typename T>
struct CubeImgView {
    ImgView<T> left, right, down, up, front, back;
};

template <typename T>
using CImg = ImgView<const T>;
template <typename T>
using CCubeImg = CubeImgView<const T>;

typedef Img<glm::vec3> Img3f;
typedef Img<glm::vec4> Img4f;
typedef Img<Color3u8> Img3u8;
typedef Img<Color4u8> Img4u8;

typedef ImgView<glm::vec3> ImgView3f;
typedef ImgView<glm::vec4> ImgView4f;
typedef ImgView<Color3u8> ImgView3u8;
typedef ImgView<Color4u8> ImgView4u8;
typedef CubeImgView<glm::vec3> CubeImgView3f;
typedef CubeImgView<glm::vec4> CubeImgView4f;
typedef CubeImgView<Color3u8> CubeImgView3u8;
typedef CubeImgView<Color4u8> CubeImgView4u8;

typedef CImg<glm::vec3> CImg3f;
typedef CImg<glm::vec4> CImg4f;
typedef CImg<Color3u8> CImg3u8;
typedef CImg<Color4u8> CImg4u8;
typedef CCubeImg<glm::vec3> CCubeImg3f;
typedef CCubeImg<glm::vec4> CCubeImg4f;
typedef CCubeImg<Color3u8> CCubeImg3u8;
typedef CCubeImg<Color4u8> CCubeImg4u8;

// --- impl -------------------------------------------------------------------------------------------------------

template <typename T>
const T& ImgView<T>::operator()(int x, int y)const {
    assert(x >= 0 && x < _w && y >= 0 && y < _h);
    return _data[x + _stride*y];
}
template <typename T>
T& ImgView<T>::operator()(int x, int y) {
    assert(x >= 0 && x < _w && y >= 0 && y < _h);
    return _data[x + _stride*y];
}

template <typename T>
ImgView<T> ImgView<T>::subImg(int x, int y, int w, int h) {
    ImgView<T> sub(w, h, _stride, _data + x + y*_stride);
    return sub;
}
template <typename T>
ImgView<const T> ImgView<T>::subImb(int x, int y, int w, int h)const {
    ImgView<const T> sub(w, h, _stride, _data + x + y*_stride);
    return sub;
}

}
