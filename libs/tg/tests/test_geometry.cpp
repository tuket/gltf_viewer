#include <tg/geometry_utils.hpp>
#include <glm/glm.hpp>
#include <tl/basic.hpp>
#include <tl/basic_math.hpp>
#include <tl/fmt.hpp>

using glm::vec2;

namespace tl
{
void toStringBufferT(FmtBuffer& buffer, glm::vec2 v)
{
    buffer.writeAndAdvance("{");
    toStringBufferT(buffer, v.x);
    buffer.writeAndAdvance(", ");
    toStringBufferT(buffer, v.y);
    buffer.writeAndAdvance("}");
}
}

bool approxEqual(vec2 a, vec2 b)
{
    const float EPS = 0.00001f;
    return tl::pow2(a.x - b.x) < EPS && tl::pow2(a.y - b.y) < EPS;
};

bool test_segmentIntersect()
{
    struct Input { vec2 a0, a1, b0, b1; };
    struct Output { int n; vec2 p[2]; };
    struct Test { Input input; Output expected; };
    auto verify = [](const Output& a, const Output& b) {
        if(a.n != b.n)
            return false;
        for(int i = 0; i < a.n; i++)
            if(!approxEqual(a.p[i], b.p[i]))
                return false;
        return true;
    };
    Test tests[] = {
        {
            Input{{0, 0}, {1, 1}, {1, 0}, {2, 1}},
            Output{0, {}}
        }, {
            Input{{2, 2}, {4, 2}, {3, 0}, {3, 3}},
            Output{1, {{3, 2}}}
        }, {
            Input{{3, 5}, {-3, -1}, {0, 5}, {3, 2}},
            Output{1, {{1.5f, 3.5f}}}
        }
    };
    bool allOkay = true;
    for(size_t i = 0; i < tl::size(tests); i++) {
        const Input& input = tests[i].input;
        Output output;
        output.n = segmentsIntersect(output.p, input.a0, input.a1, input.b0, input.b1);
        if(!verify(output, tests[i].expected)) {
            auto printOutput = [](const Output& o)
            {
                tl::print("{");
                for(int i = 0; i < o.n; i++) {
                    tl::print("{", o.p[i].x, ", ",  o.p[i].y ,"}");
                    if(i != o.n)
                        tl::print(", ");
                }
                tl::println("}");
            };
            tl::println(i, ") fail");
            tl::println("got: ");
            printOutput(output);
            tl::println("expected: ");
            printOutput(tests[i].expected);
            allOkay = false;
        }
    }
    if(allOkay) {
        tl::println("All tests passed");
    }
    return allOkay;
}
