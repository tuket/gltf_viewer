#include "geometry_utils.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <tl/containers/fvector.hpp>
#include <tl/basic_math.hpp>
#include <tl/basic.hpp>
#include <tl/bitset.hpp>

using glm::vec2;

#include <tl/fmt.hpp>
static int traceX, traceY;
template <typename... Args>
void trace(const Args&... args)
{
    if(traceX == 128 && traceY == 107)
        tl::print(args...);
}
namespace tl
{
static void toStringBufferT(FmtBuffer& buffer, glm::vec2 v)
{
    buffer.writeAndAdvance("{");
    toStringBufferT(buffer, v.x);
    buffer.writeAndAdvance(", ");
    toStringBufferT(buffer, v.y);
    buffer.writeAndAdvance("}");
}
}

namespace tg
{

u8 segmentsIntersect(vec2 *out, vec2 a0, vec2 a1, vec2 b0, vec2 b1)
{
    constexpr float EPSILON = 0.0001f;
    const float alphaDenominator =
        (a0.x - a1.x) * (b1.y - b0.y) -
        (a0.y - a1.y) * (b1.x - b0.x);
    const float alphaNumerator =
        (b1.x - a1.x) * (b1.y - b0.y) -
        (b1.y - a1.y) * (b1.x - b0.x);
    const float betaDenominator =
        (a0.y - a1.y) * (b1.x - b0.x) -
        (a0.x - a1.x) * (b1.y - b0.y);
    const float betaNumerator =
        (b1.x - a1.x) * (a0.y - a1.y) -
        (b1.y - a1.y) * (a0.x - a1.x);

    if(alphaDenominator * alphaDenominator > EPSILON)
    {
        // segments are NOT parallel
        const float alpha = alphaNumerator / alphaDenominator;
        if(alpha < 0 || alpha > 1)
            return 0;

        const float beta = betaNumerator / betaDenominator;
        if(beta < 0 || beta > 1)
            return 0;

        out[0] = mix(a1, a0, alpha);
        return 1;
    }
    else // segments are parallel
    {
        if(alphaNumerator * alphaNumerator > EPSILON) // segments don't lie on the same line
            return 0;
        // return -1 if before the segment, 0 if in between, and +1 if after
        auto pointRelPos = [](vec2 p, vec2 seg0, vec2 seg1)
        {
            if(dot(seg1-seg0, p-seg0) < 0)
                return -1;
            if(glm::distance2(seg0, p) > glm::distance2(seg0, seg1))
                return +1;
            return 0;
        };
        const int relPosB0 = pointRelPos(b0, a0, a1);
        const int relPosB1 = pointRelPos(b1, a0, a1);
        if(relPosB0 == -1)
        {
            if(relPosB1 == -1)
                return 0;
            else {
                out[0] = a0;
                if(relPosB1 == 0)
                    out[1] = b1;
                else
                    out[1] = a1;
            }
        }
        else if(relPosB0 == 0)
        {
            out[0] = b0;
            if(relPosB1 == 0)
                out[1] = b1;
            else
                out[1] = a1;
        }
        else // relPosB0 == 1
        {
            return 0;
        }

        if(out[0] == out[0])
            return 1;
        else
            return 2;
    }
}

// points in a quad must be in CCW order
bool isPointInsideQuad(glm::vec2 p, tl::CSpan<glm::vec2> q)
{
    auto turn90DegCW = [](vec2 p) -> vec2{
        return {p.y, -p.x};
    };
    const int n = (int)q.size();
    for(int i = 0; i < n; i++) {
        if(dot(turn90DegCW(q[(i+1)%n] - q[i]), p - q[i]) >= 0)
            return false;
    }
    return true;
}

bool isPointInsideRect(vec2 p, const tl::rect& r)
{
    return p.x > r.pMin.x && p.x < r.pMax.x &&
           p.y > r.pMin.y && p.y < r.pMax.y;
}

float triangleArea(vec2 a, vec2 b, vec2 c)
{
    const vec2 v1 = b - a;
    const vec2 v2 = c - a;
    const vec2 v1p = {v1.y, -v1.x};
    return fabsf(0.5f * dot(v1p, v2));
}

float convexPolyArea(tl::CSpan<vec2> poly)
{
    if(poly.size() < 3)
        return 0;
    const size_t n = poly.size();
    // sum the area for the fan triangles
    float area = triangleArea(poly[0], poly[n-2], poly[n-1]);
    for(size_t i1 = 1, i2 = 2; i2 < n; i1++, i2++)
        area += triangleArea(poly[0], poly[i1], poly[i2]);
    return area;
}

float intersectionArea_square_quad(const tl::rect& s, tl::CSpan<vec2> q, int myTraceX, int myTraceY)
{
    traceX = myTraceX;
    traceY = myTraceY;
    assert(q.size() == 4);
    const vec2 sp[4] = {
        s.pMin,
        {s.pMax.x, s.pMin.y},
        s.pMax,
        {s.pMin.x, s.pMax.y}
    };
    auto edgePos0 = [&](int e) {
        return e < 4 ? sp[e] : q[e-4];
    };
    auto edgePos1 = [&](int e) {
        if(e < 4)
            return sp[(e+1) % 4];
        else
            return q[(e-4+1) % 4];
    };
    auto calcNextEdge = [](int e) {
        if(e < 4)
            return (e+1) % 4;
        else
            return 4 + (e-4+1) % 4;
    };
    const tl::Bitset8 pInside = {
        isPointInsideRect(q[0], s),
        isPointInsideRect(q[1], s),
        isPointInsideRect(q[2], s),
        isPointInsideRect(q[3], s),
        isPointInsideQuad(sp[0], q),
        isPointInsideQuad(sp[1], q),
        isPointInsideQuad(sp[2], q),
        isPointInsideQuad(sp[3], q)
    };
    u8 intersectMtx[4][4];
    vec2 intersectPoints[8];
    u8 numIntersectPoints = 0;
    for(int i = 0; i < 4; i++)
    for(int j = 0; j < 4; j++)
    {
        intersectMtx[i][j] = 0xFF;
        const u8 n = segmentsIntersect(&intersectPoints[numIntersectPoints],
            sp[i], sp[(i+1)%4], q[j], q[(j+1)%4]);
        if(n > 0)
        {
            if(n == 1)
                intersectMtx[i][j] = numIntersectPoints;
            else //if(n == 2)
                intersectMtx[i][j] = numIntersectPoints | (u8)((numIntersectPoints+1) << 4);
            numIntersectPoints += n;
        }
    }
    if(pInside & (u8)0b1111) { // square inside quad
        const float w = s.pMax.x - s.pMin.x;
        return w*w;
    }
    if(pInside & (u8)0b11110000) { // quad inside square
        return triangleArea(q[0], q[1], q[2]) + triangleArea(q[2], q[3], q[0]);
    }

    tl::FVector<vec2, 8> poly; // here we compute the intersection polygon
    u8 curEdge = 0xFF;
    u8 curPoint = 0xFF;
    [&](){
        for(i8 i = 0; i < 8; i++)
        if(pInside[i])
        {
            curPoint = i;
            curEdge = i;
            return;
        }
        for(i8 i = 0; i < 4; i++)
        for(i8 j = 0; j < 4; j++)
        for(i8 k = 0; k < 2; k++)
        {
            if((intersectMtx[i][j] >> 4*k) & 0xFF)
        }
    }();

    const float area = convexPolyArea(poly);
    return area;
}

i8 calcPointSideWrtLine(glm::vec2 s0, glm::vec2 s1, glm::vec2 a)
{
    const vec2 s = s1 - s0;
    const vec2 s_ = {s.y, -s.x}; // turn 90 degrees clock-wise
    const float d = dot(s_, a-s0);
    return d < 0 ? -1 :
           d > 0 ? +1 : 0;
}

}
