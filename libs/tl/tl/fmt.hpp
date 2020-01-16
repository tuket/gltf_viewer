#pragma once

#include "str.hpp"
#include <stdio.h>
#include <string.h>

namespace tl
{

class FmtBuffer
{
    char* start;
    char* end;
    i32 size;
public:
    FmtBuffer(char* start, i32 size = -1)
        : start(start), end(start), size(size)
    {}
    u32 count()const {
        return end - start;
    }

    char& operator[](i32 i) {
        assert(size < 0 || i < size);
        return end[i];
    }

    void advance(u32 advance) {
        end += advance;
        size -= advance;
    }

    void writeAndAdvance(char c) {
        if(size != 0) {
            if(start)
                *end = c;
            end++;
            size--;
        }
    }
    void writeAndAdvance(CStr x) {
        if(size != 0) {
            if(start)
                memcpy(end, x.begin(), x.size());
            end += x.size();
            size -= x.size();
        }
    }
    void addNullTerminator() {
        if(size == 0)
            *(end-1) = '\0';
        else {
            *end = '\0';
            end++;
            size--;
        }
    }
};

template <typename T>
static u32 calcToStringLengthT(T x) {
    FmtBuffer buf = nullptr;
    toStringBufferT(buf, x);
    return buf.count();
}

// --- strings ------------------------------------------------------------

u32 calcToStringLengthT(CStr str);
void toStringBufferT(FmtBuffer& buffer, CStr str);

static u32 calcToStringLengthT(const char* str) { return calcToStringLengthT(CStr(str)); }
static void toStringBufferT(FmtBuffer& buffer, const char* str) { toStringBufferT(buffer, CStr(str)); }

template <u32 N>
static u32 calcToStringLengthT(const char (&str)[N]) { return calcToStringLengthT(CStr(str)); }
template <u32 N>
static void toStringBufferT(FmtBuffer& buffer, const char (&str)[N]) { toStringBufferT(buffer, CStr(str)); }

/*
static u32 calcToStringLength(const char* str) { return calcToStringLength(CStr(str)); }
static void toStringBuffer(char* buffer, const char* str) { toStringBuffer(buffer, CStr(str)); }

static u32 calcToStringLength(char* str) { return calcToStringLength(CStr(str)); }
static void toStringBuffer(char* buffer, char* str) { toStringBuffer(buffer, CStr(str)); }*/

// --- double -------------------------------------------------------------

/*
struct Fmt_double;

class Formatter_double
{
public:
    Formatter_double();
    Fmt_double operator()(double x)const;
private:
    int _maxDecimalPlaces;
    int _fixedDecimalPlaces;
};

struct Fmt_double
{
    double value;
    Formatter_double formatter;
};

namespace fmt
{
Fmt_double maxDecimalPlaces(double x, );
}
*/

// --- int ----------------------------------------------------------------------------

class Fmt_int;

class Formatter_int
{
public:
    Fmt_int operator()(i8 x)const;
    Fmt_int operator()(i16 x)const;
    Fmt_int operator()(i32 x)const;
    Fmt_int operator()(i64 x)const;
    Fmt_int operator()(u8 x)const;
    Fmt_int operator()(u16 x)const;
    Fmt_int operator()(u32 x)const;
    Fmt_int operator()(u64 x)const;

    int _base;
    bool _capital;
};

class Fmt_int
{
public:
    Formatter_int _formatter;
    u64 _value;
    int _negative; // 0: positive, 1: negative
};

namespace fmt
{
    Formatter_int base(int base, bool capital = true);
    Formatter_int hex(bool capital = true);
    Formatter_int octal();
    Formatter_int binary();
}

u32 calcToStringLengthT(const Fmt_int& fmt);
void toStringBufferT(FmtBuffer& buffer, const Fmt_int& fmt);

u32 calcToStringLengthT(i8 x);
u32 calcToStringLengthT(i16 x);
u32 calcToStringLengthT(i32 x);
u32 calcToStringLengthT(i64 x);
u32 calcToStringLengthT(u8 x);
u32 calcToStringLengthT(u16 x);
u32 calcToStringLengthT(u32 x);
u32 calcToStringLengthT(u64 x);
void toStringBufferT(FmtBuffer& buffer, i8 x);
void toStringBufferT(FmtBuffer& buffer, i16 x);
void toStringBufferT(FmtBuffer& buffer, i32 x);
void toStringBufferT(FmtBuffer& buffer, i64 x);
void toStringBufferT(FmtBuffer& buffer, u8 x);
void toStringBufferT(FmtBuffer& buffer, u16 x);
void toStringBufferT(FmtBuffer& buffer, u32 x);
void toStringBufferT(FmtBuffer& buffer, u64 x);

// --- toString -------------------------------------------------------------------------

template<typename... T>
static u32 calcToStringLength(const T&... x)
{
    return (calcToStringLengthT(x) + ...);
}

template <typename... Ts>
static void fmtBufferAppend(FmtBuffer&, const Ts&...)
{}


template <typename T, typename... Ts>
static void fmtBufferAppend(FmtBuffer& buffer, const T& x, const Ts&... xs)
{
    toStringBufferT(buffer, x);
    fmtBufferAppend(buffer, xs...);
}

template <typename T, typename... Ts>
static void toFmtBuffer(FmtBuffer& buffer, const T& x, const Ts&... xs)
{
    fmtBufferAppend(buffer, x, xs...);
    buffer.addNullTerminator();
}

template <typename T, typename... Ts>
static void toStringBuffer(char* buffer, int bufferSize, const T& x, const Ts&... xs)
{
    FmtBuffer buf(buffer, bufferSize);
    fmtBufferAppend(buf, x, xs...);
    buf.addNullTerminator();
}

/*template <typename T, typename... Ts>
static void toStringBuffer(tl::Buffer& buffer, const T& x, const Ts&... xs)
{
    toStringBuffer(buffer.data(), buffer.size(), x, xs...);
}*/

template <int BUF_SIZE, typename T, typename... Ts>
static void toStringBuffer(char (&buffer)[BUF_SIZE], const T& x, const Ts&... xs)
{
    toStringBuffer(&buffer[0], BUF_SIZE, x, xs...);
}

template<typename... T>
static Str toString(const T&... x)
{
    const u32 size = calcToStringLength(x...);
    Str str(size);
    FmtBuffer buffer(str.begin(), size+1);
    tl::toFmtBuffer<T...>(buffer, x...);
    return str;
}

// --- print --------------------------------------------------------------------------

void printCStr(CStr str);
void printlnCStr(CStr str);

template <typename... Args>
static void print(const Args&... args)
{
    Str str = toString(args...);
    printCStr(CStr(str));
}

template <typename... Args>
static void println(const Args&... args)
{
    Str str = toString(args...);
    printlnCStr(CStr(str));
}

void fprintCStr(FILE* file, CStr str);
void fprintlnCStr(FILE* file, CStr str);

template <typename... Args>
static void fprint(FILE* file, const Args&... args)
{
    Str str = toString(args...);
    fprintCStr(file, CStr(str));
}

template <typename... Args>
static void fprintln(FILE* file, const Args&... args)
{
    fprint(file, args..., "\n");
}

template <typename... Args>
static void eprint(const Args&... args)
{
    fprint(stderr, args...);
}

template <typename... Args>
static void eprintln(const Args&... args)
{
    fprint(stderr, args..., "\n");
}

template <typename... Args>
static void printError(const Args&... args)
{
    fprint(stderr, args..., "\n");
}

template <typename... Args>
static void sprint(Str& out, const Args&... args)
{
    const u32 prevSize = out.size();
    const u32 newSize = prevSize + calcToStringLength(args...);
    out.resize(newSize);
    toStringBuffer(&out[prevSize], args...);
}

template <typename... Args>
static void sprintln(Str& out, const Args&... args)
{
    sprint(out, args..., "\n");
}

}
