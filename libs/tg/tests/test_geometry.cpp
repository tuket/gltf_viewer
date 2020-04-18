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

