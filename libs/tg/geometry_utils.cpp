#include "geometry_utils.hpp"

#include <glm/glm.hpp>
#include <tl/containers/fvector.hpp>
#include <tl/basic_math.hpp>
#include <tl/basic.hpp>

using glm::vec2;

bool segmentIntersect(vec2& out, vec2 a0, vec2 a1, vec2 b0, vec2 b1)
{
    const float alphaDenominator =
        (a0.x - a1.x) * (b1.y - b0.y) -
        (a0.y - a1.y) * (b1.x - b0.x);
    if(alphaDenominator * alphaDenominator < 0.01f)
        return false;

    const float betaDenominator =
        (a0.y - a1.y) * (b1.x - b0.x) -
        (a0.x - a1.x) * (b1.y - b0.y);
    if(betaDenominator * betaDenominator < 0.01f)
        return false;

    const float alphaNumerator =
        (b1.x - a1.x) * (b1.y - b0.y) -
        (b1.y - a1.y) * (b1.x - b0.x);
    const float alpha = alphaNumerator / alphaDenominator;
    if(alpha < 0 || alpha > 1)
        return false;

    const float betaNumerator =
        (b1.x - a1.x) * (a0.y - a1.y) -
        (b1.y - a1.y) * (a0.x - a1.x);
    const float beta = betaNumerator / betaDenominator;
    if(beta < 0 || beta > 1)
        return false;

    out = mix(a1, a0, alpha);
    return true;
}

// points in a quad must be in CCW order
bool isPointInsideQuad(glm::vec2 p, tl::CSpan<glm::vec2> q)
{
    auto turn90DegCW = [](vec2 p) -> vec2{
        return {p.y, -p.x};
    };
    for(int i = 0; i < (int)q.size(); i++) {
        if(dot(turn90DegCW(q[1] - q[0]), p - q[0]) >= 0)
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
    return abs(0.5f * dot(v1p, v2));
}

float intersectionArea_square_quad(const tl::rect& s, tl::CSpan<glm::vec2> q)
{
    assert(q.size() == 4);
    const vec2 sp[4] = { // square points;
        s.pMin,
        {s.pMin.x, s.pMax.y},
        {s.pMax.x, s.pMin.y},
        s.pMax
    };
    const bool sInsideQ[4] = {
        isPointInsideQuad(sp[0], q),
        isPointInsideQuad(sp[1], q),
        isPointInsideQuad(sp[2], q),
        isPointInsideQuad(sp[3], q)
    };
    const bool qInsideS[4] = {
        isPointInsideRect(q[0], s),
        isPointInsideRect(q[1], s),
        isPointInsideRect(q[2], s),
        isPointInsideRect(q[3], s),
    };
    if(sInsideQ[0] && sInsideQ[1] && sInsideQ[2] && sInsideQ[3]) { // square inside quad
        const float w = s.pMax.x - s.pMin.x;
        return w*w;
    }
    else if(qInsideS[0] && qInsideS[1] && qInsideS[2] && qInsideS[3]) { // quad inside square
        return triangleArea(q[0], q[1], q[2]) + triangleArea(q[2], q[3], q[0]);
    }
    else {
        const vec2* q1;
        const vec2* q2;
        int i = [&]() {
            for(int i = 0; i < 4; i ++)
                if(sInsideQ[i]) {
                    q1 = sp;
                    q2 = q.begin();
                    return i;
                }
            for(int i = 0; i < 4; i++)
                if(qInsideS[i]) {
                    q1 = q.begin();
                    q2 = sp;
                    return i;
                }
        }();
        tl::FVector<vec2, 8> areaPoly; // the points that define the the polygon of the intersection
        tl::FVector<vec2, 2> intersectionPoints; // temp intersection points of two lines
        const int end = (i + 4) % 4;
        do {
            const int i1 = (i + 1) % 4;
            intersectionPoints.clear();
            for(int j = 0; j < 4; j++) {
                const int j1 = (j+1) % 4;
                if(segmentIntersect(intersectionPoints.back(), q1[i], q1[i1], q2[j], q2[j1])) {
                    intersectionPoints.resize(intersectionPoints.size() + 1);
                }
            }
            i++;
        } while(i != end);
    }
}
