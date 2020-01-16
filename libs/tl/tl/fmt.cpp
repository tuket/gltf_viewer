#include "fmt.hpp"

#include <math.h>
#include <stdio.h>

namespace tl
{

// --- strings ------------------------------------------------------------------

u32 calcToStringLengthT(CStr str)
{
    return str.size();
}

void toStringBufferT(FmtBuffer& buffer, CStr str)
{
    buffer.writeAndAdvance(str);
}

// --- int ----------------------------------------------------------------------

static const char baseSymbols[2][36] =
{
    {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
    },
    {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
    }
};

u32 calcToStringLengthT(const Fmt_int& fmtInt)
{
    const int base = fmtInt._formatter._base;
    assert(base <= 36);
    u32 len = fmtInt._negative;
    u64 x = fmtInt._value;
    for(int y = x; y; len++, y /= base);
    return len > 0 ? len : 1;
}

void toStringBufferT(FmtBuffer& buffer, const Fmt_int& fmtInt)
{
    const char* symbols = baseSymbols[fmtInt._formatter._capital];
    const int base = fmtInt._formatter._base;
    u64 x = fmtInt._value;
    int neg = 0;
    if(fmtInt._negative) {
        buffer.writeAndAdvance('-');
        neg = 1;
    }
    if(x == 0)
        buffer.writeAndAdvance('0');
    const u32 len = calcToStringLengthT(fmtInt);
    for(int i = len-1; x; x /= base, i--) {
        const u64 s = x % base;
        buffer[i] = symbols[s];
    }
    buffer.advance(len);
}

u32 calcToStringLengthT(i8 x) { return calcToStringLengthT(fmt::base(10)(x)); }
u32 calcToStringLengthT(i16 x) { return calcToStringLengthT(fmt::base(10)(x)); }
u32 calcToStringLengthT(i32 x) { return calcToStringLengthT(fmt::base(10)(x)); }
u32 calcToStringLengthT(i64 x) { return calcToStringLengthT(fmt::base(10)(x)); }
u32 calcToStringLengthT(u8 x) { return calcToStringLengthT(fmt::base(10)(x)); }
u32 calcToStringLengthT(u16 x) { return calcToStringLengthT(fmt::base(10)(x)); }
u32 calcToStringLengthT(u32 x) { return calcToStringLengthT(fmt::base(10)(x)); }
u32 calcToStringLengthT(u64 x) { return calcToStringLengthT(fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, i8 x) { toStringBufferT(buffer, fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, i16 x) { toStringBufferT(buffer, fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, i32 x) { toStringBufferT(buffer, fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, i64 x) { toStringBufferT(buffer, fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, u8 x) { toStringBufferT(buffer, fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, u16 x) { toStringBufferT(buffer, fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, u32 x) { toStringBufferT(buffer, fmt::base(10)(x)); }
void toStringBufferT(FmtBuffer& buffer, u64 x) { toStringBufferT(buffer, fmt::base(10)(x)); }

#define SPECIALIZE_SIGNED_FORMATTER(Int)                        \
    Fmt_int Formatter_int::operator()(Int x)const               \
    {                                                           \
        Fmt_int fmt;                                            \
        fmt._formatter = *this;                                 \
        if(x >= 0) {                                            \
            fmt._negative = false;                              \
            fmt._value = x;                                     \
        }                                                       \
        else {                                                  \
            fmt._negative = true;                               \
            fmt._value = -x;                                    \
        }                                                       \
        return fmt;                                             \
    }

SPECIALIZE_SIGNED_FORMATTER(i8);
SPECIALIZE_SIGNED_FORMATTER(i16);
SPECIALIZE_SIGNED_FORMATTER(i32);
SPECIALIZE_SIGNED_FORMATTER(i64);

#define SPECILIZE_UNSINGED_FORMATTER(Int)                       \
    Fmt_int Formatter_int::operator()(Int x)const               \
    {                                                           \
        Fmt_int fmt;                                            \
        fmt._formatter = *this;                                 \
        fmt._negative = false;                                  \
        fmt._value = x;                                         \
        return fmt;                                             \
    }

SPECILIZE_UNSINGED_FORMATTER(u8)
SPECILIZE_UNSINGED_FORMATTER(u16)
SPECILIZE_UNSINGED_FORMATTER(u32)
SPECILIZE_UNSINGED_FORMATTER(u64)

namespace fmt
{
    Formatter_int base(int base, bool capital)
    {
        assert(base >= 2 && base <= 36);
        Formatter_int fmt;
        fmt._base = base;
        fmt._capital = capital;
        return fmt;
    }

    Formatter_int hex(bool capital)
    {
        return base(16, capital);
    }

    Formatter_int octal()
    {
        return base(8, false);
    }

    Formatter_int binary()
    {
        return base(2, false);
    }
}

// --- print ----------------------------------------------------------------------------

void printCStr(CStr str)
{
    fputs(str.c_str(), stdout);
}

void printlnCStr(CStr str)
{
    puts(str.c_str());
}

void fprintCStr(FILE* file, CStr str)
{
    fputs(str.c_str(), file);
}

void fprintlnCStr(FILE* file, CStr str)
{
    fprintf(file, "%s\n", str.c_str());
}

}
