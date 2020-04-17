#include "geometry_utils.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <tl/containers/fvector.hpp>
#include <tl/basic_math.hpp>
#include <tl/basic.hpp>
#include <tl/bitset.hpp>

using glm::vec2;

#include <tl/fmt.hpp>

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
    float area = 0;
    for(size_t i1 = 1, i2 = 2; i2 < n; i1++, i2++)
        area += triangleArea(poly[0], poly[i1], poly[i2]);
    if(area > 1.2)
        printf("blabla\n");
    return area;
}

float intersectionArea_square_quad(const tl::rect& s, tl::CSpan<vec2> q)
{
    tl::FVector<vec2, 8> poly0; // here we compute the intersection polygon
    tl::FVector<vec2, 8> poly1;

    // EDGE 0
    for(i8 iq = 0; iq < 4; iq++)
    {
        const vec2 a = q[iq];
        const vec2 b = q[(iq+1)%4];
        if(a.y >= s.yMin)
            poly0.push_back(a);
        if((a.y < s.yMin && b.y > s.yMin) ||
           (a.y > s.yMin && b.y < s.yMin))
        {
            vec2 p;
            p.y = s.yMin;
            const float alpha = (p.y - a.y) / (b.y - a.y);
            p.x = glm::mix(a.x, b.x, alpha);
            poly0.push_back(p);
        }
    }

    // EDGE 1
    i8 n = (i8)poly0.size();
    for(i8 iq = 0; iq < n; iq++)
    {
        const vec2 a = poly0[iq];
        const vec2 b = poly0[(iq+1)%n];
        if(a.x <= s.xMax)
            poly1.push_back(a);
        if((a.x < s.xMax && b.x > s.xMax) ||
           (a.x > s.xMax && b.x < s.xMax))
        {
            vec2 p;
            p.x = s.xMax;
            const float alpha = (p.x - a.x) / (b.x - a.x);
            p.y = glm::mix(a.y, b.y, alpha);
            poly1.push_back(p);
        }
    }

    // EDGE 2
    n = (i8)poly1.size();
    poly0.clear();
    for(i8 iq = 0; iq < n; iq++)
    {
        const vec2 a = poly1[iq];
        const vec2 b = poly1[(iq+1)%n];
        if(a.y <= s.yMax)
            poly0.push_back(a);
        if((a.y < s.yMax && b.y > s.yMax) ||
           (a.y > s.yMax && b.y < s.yMax))
        {
            vec2 p;
            p.y = s.yMax;
            const float alpha = (p.y - a.y) / (b.y - a.y);
            p.x = glm::mix(a.x, b.x, alpha);
            poly0.push_back(p);
        }
    }

    // EDGE 3
    n = (i8)poly0.size();
    poly1.clear();
    for(i8 iq = 0; iq < n; iq++)
    {
        const vec2 a = poly0[iq];
        const vec2 b = poly0[(iq+1)%n];
        if(a.x >= s.xMin)
            poly1.push_back(a);
        if((a.x > s.xMin && b.x < s.xMin) ||
           (a.x < s.xMin && b.x > s.xMin))
        {
            vec2 p;
            p.x = s.xMin;
            const float alpha = (p.x - a.x) / (b.x - a.x);
            p.y = glm::mix(a.y, b.y, alpha);
            poly1.push_back(p);
        }
    }

    const float area = convexPolyArea(poly1);
    if(false) // test agains brute force
    {
        const float approxArea = [&]()
        {
            const int n = 30;
            int count = 0;
            for(int iy = 0; iy < n; iy++)
            for(int ix = 0; ix < n; ix++)
            {
                const float y = s.yMin + (float)iy / n;
                const float x = s.xMin + (float)ix / n;
                if(isPointInsideQuad({x, y}, q))
                    count++;
            }
            const float area = (float)count / (n*n);
            return area;
        }();
        if(fabsf(area - approxArea) > 0.05f) {
            printf("fail!\n");
        }
    }
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
