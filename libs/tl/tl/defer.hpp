#pragma once

template <typename F>
struct _Defer {
    F f;
    _Defer(F f) : f(f) {}
    ~_Defer() { f(); }
};

template <typename F>
_Defer<F> _defer_func(F f) {
    return _Defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = _defer_func([&](){code;})
