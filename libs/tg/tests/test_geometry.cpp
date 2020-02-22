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

bool test_segmentIntersect()
{
    struct Input { vec2 a0, a1, b0, b1; };
    struct Output { bool b; vec2 p; };
    struct Test { Input input; Output expected; };
    auto verify = [](const Output& a, const Output& b) {
        if(a.b != b.b)
            return false;
        if(a.b == false)
            return true;
        auto approxEqual = [](vec2 a, vec2 b) {
            const float EPS = 0.001f;
            return tl::pow2(a.x - b.x) < EPS && tl::pow2(a.y - b.y) < EPS;
        };
        return approxEqual(a.p, b.p);
    };
    Test tests[] = {
        {
            Input{{0, 0}, {1, 1}, {1, 0}, {2, 1}},
            Output{false, {}}
        }, {
            Input{{2, 2}, {4, 2}, {3, 0}, {3, 3}},
            Output{true, {3, 2}}
        }, {
            Input{{3, 5}, {-3, -1}, {0, 5}, {3, 2}},
            Output{true, {1.5f, 3.5f}}
        }
    };
    bool allOkay = true;
    for(size_t i = 0; i < tl::size(tests); i++) {
        const Input& input = tests[i].input;
        Output output;
        output.b = segmentIntersect(output.p, input.a0, input.a1, input.b0, input.b1);
        if(!verify(output, tests[i].expected)) {
            tl::println(i, ") fail");
            tl::println("got: ", output.b ? "true" : "false", " ", output.p);
            tl::println("expected: ", tests[i].expected.b ? "true" : "false", " ", tests[i].expected.p);
            allOkay = false;
        }
    }
    if(allOkay) {
        tl::println("All tests passed");
    }
    return allOkay;
}
