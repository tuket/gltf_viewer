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
    const vec2 sp[4] = {
        s.pMin,
        {s.pMax.x, s.pMin.y},
        s.pMax,
        {s.pMin.x, s.pMax.y}
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
    auto calcSlopeType = [](vec2 a) -> u16 {
        const i8 x = tl::sign(a.x);
        const i8 y = tl::sign(a.y);
        if(x == 0 || y == 0) // '.'
            return 0;
        else if(x == 0) // '|'
            return 1;
        else if(y == 0) // '-'
            return 2;
        else if(x == y) // '/'
            return 3;
        else // '\'
            return 4;
    };
    const u16 qSlopeBits = (u16) (
        calcSlopeType(q[0]) |
        calcSlopeType(q[1]) << 4 |
        calcSlopeType(q[2]) << 8 |
        calcSlopeType(q[3]) << 12 );
    auto qSlope = [&qSlopeBits](u8 iq) -> u8
    {
        return (qSlopeBits >> (4*iq)) & 0xF;
    };

    if((pInside & (u8)0b1111) == 0b1111) { // square inside quad
        const float w = s.pMax.x - s.pMin.x;
        return w*w;
    }
    if((pInside & (u8)0b11110000) == 0b11110000) { // quad inside square
        return triangleArea(q[0], q[1], q[2]) + triangleArea(q[0], q[2], q[3]);
    }

    tl::FVector<vec2, 8> poly; // here we compute the intersection polygon

    // EDGE 0
    if(pInside[4])
    {
        poly.push_back(sp[0]);
        i8 intersectedEdge = -1;
        for(i8 iq = 0; iq < 4; iq++) {
            const u8 qs = qSlope(iq);
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            float yMin = a.y,
                  yMax = b.y;
            tl::minMax(yMin, yMax);
            if(s.yMin <= yMin || s.yMin >= yMax)
                continue;
            if(qs == 1) { // '|'
                if(a.x > s.xMin && a.x < s.xMax) {
                    poly.push_back({a.x, s.yMin});
                    intersectedEdge = iq;
                    break;
                }
            } else if(qs == 3 || qs == 4) { // '/' || '\'
                vec2 p;
                p.y = s.yMin;
                const float alpha = (p.y - a.y) / (b.y - a.y);
                p.x = glm::mix(a.x, b.x, alpha);
                if(p.x > s.xMin && p.x < s.xMax) {
                    poly.push_back(p);
                    intersectedEdge = iq;
                    break;
                }
            }
        }
        if(intersectedEdge != -1) {
            i8 iq = (intersectedEdge + 1) % 4;
            while(pInside[iq]) {
                poly.push_back(q[iq]);
                iq = (iq+1) % 4;
            }
        }
    }
    else // if(!pInside[4])
    {
        vec2 intersecPoints[2];
        i8 numInter = 0;
        for(i8 iq = 0; iq < 4 && numInter < 2; iq++) {
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            const u8 abSlope = qSlope(iq);
            if(abSlope != 0 && abSlope != 2) { // !'.' && '-'
                float minY = a.y,
                      maxY = b.y;
                tl::minMax(minY, maxY);
                if(s.yMin > minY && s.yMin < maxY) {
                    vec2 p;
                    p.y = s.yMin;
                    const float alpha = (p.y - a.y) / (b.y - a.y);
                    p.x = glm::mix(a.x, b.x, alpha);
                    if(p.x > s.xMin && p.x < s.xMax) {
                        intersecPoints[numInter] = p;
                        numInter++;
                    }
                }
            }
        }
        if(numInter == 1) {
            poly.push_back(intersecPoints[0]);
        } else { // numInter == 2
            if(intersecPoints[0].x > intersecPoints[1].x) {
                tl::swap(intersecPoints[0], intersecPoints[1]);
            }
            poly.push_back(intersecPoints[0]);
            poly.push_back(intersecPoints[1]);
        }
    }

    // EDGE 1
    if(pInside[5])
    {
        poly.push_back({s.xMax, s.yMin});
        i8 intersectedEdge = -1;
        for(i8 iq = 0; iq < 4; iq++) {
            const u8 qs = qSlope(iq);
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            float xMin = a.x,
                  xMax = b.x;
            tl::minMax(xMin, xMax);
            if(s.xMax <= xMin || s.xMax >= xMax)
                continue;
            if(qs == 2) { // '-'
                if(a.y > s.yMin && a.y < s.yMax) {
                    poly.push_back({s.xMax, a.y});
                    intersectedEdge = iq;
                    break;
                }
            }
            else if(qs == 3 || qs == 4) { // '/' || '\'
                vec2 p;
                p.x = s.xMax;
                const float alpha = (p.x - a.x) / (b.x - a.x);
                p.y = glm::mix(a.y, b.y, alpha);
                if(p.y > s.yMax && p.y < s.xMax) {
                    poly.push_back(p);
                    intersectedEdge = iq;
                    break;
                }
            }
        }
        if(intersectedEdge != -1) {
            i8 iq = (intersectedEdge+1) % 4;
            while(pInside[iq]) {
                poly.push_back(q[iq]);
                iq = (iq+1) % 4;
            }
        }
    }
    else //!pInside[5]
    {
        vec2 intersecPoints[2];
        i8 numInter = 0;
        for(i8 iq = 0; iq < 4; iq++) {
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            const u8 abSlope = qSlope(iq);
            if(abSlope != 0 && abSlope != 1) { // !'.' && !'|'
                float minX = a.x,
                      maxX = b.x;
                tl::minMax(minX, maxX);
                if(s.xMax > minX && s.xMax < maxX) {
                    vec2 p;
                    p.x = s.xMax;
                    const float alpha = (p.x - a.x) / (b.x - a.x);
                    p.y = glm::mix(a.y, a.x, alpha);
                    if(p.y > s.yMin && p.y < s.yMax) {
                        intersecPoints[numInter] = p;
                        numInter++;
                    }
                }
            }
        }
        if(numInter == 1) {
            poly.push_back(intersecPoints[0]);
        }
        else if(numInter == 2) {
            if(intersecPoints[0].y > intersecPoints[1].y) {
                tl::swap(intersecPoints[0], intersecPoints[1]);
            }
            poly.push_back(intersecPoints[0]);
            poly.push_back(intersecPoints[1]);
        }
    }

    // EDGE 2
    if(pInside[6])
    {
        poly.push_back(q[2]);
        i8 intersectedEdge = -1;
        for(i8 iq = 0; iq < 4; iq++)
        {
            const u8 qs = qSlope(iq);
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            float yMin = a.y,
                  yMax = b.y;
            tl::minMax(yMin, yMax);
            if(s.yMax <= yMin || s.yMax >= yMax)
                continue;
            if(qs == 1) { // '|'
                if(a.x > s.xMin || a.x < s.xMax) {
                    poly.push_back({a.x, s.yMin});
                    intersectedEdge = iq;
                    break;
                }
            }
            else if(qs == 2 || qs == 3) { // '/' || '\'
                vec2 p;
                p.y = s.yMax;
                const float alpha = (p.y - a.y) / (b.y - a.y);
                p.x = glm::mix(a.x, b.x, alpha);
                if(p.x > s.xMin && p.y < s.xMax) {
                    poly.push_back(p);
                    intersectedEdge = iq;
                    break;
                }
            }
            if(intersectedEdge != -1) {
                i8 iq = (intersectedEdge + 1) % 4;
                while(pInside[iq]) {
                    poly.push_back(q[iq]);
                    iq = (iq + 1) % 4;
                }
            }
        }
    }
    else // !pInside[6]
    {
        vec2 intersecPoints[2];
        i8 numInter = 0;
        for(i8 iq = 0; iq < 4; iq++) {
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            const u8 abSlope = qSlope(iq);
            if(abSlope == 0 || abSlope == 2) // '.' || '-'
                continue;
            float yMin = a.y,
                  yMax = b.y;
            tl::minMax(yMin, yMax);
            if(s.yMax > yMin && s.yMax < yMax) {
                vec2 p;
                p.y = s.yMax;
                const float alpha = (p.y - a.y) / (b.y - a.y);
                p.x = glm::mix(a.x, b.x, alpha);
                if(p.x > s.xMin && p.x < s.xMax) {
                    intersecPoints[numInter] = p;
                    numInter++;
                }
            }
        }
        if(numInter == 1) {
            poly.push_back(intersecPoints[0]);
        }
        else if(numInter == 2) {
            if(intersecPoints[0].x < intersecPoints[1].x) {
                tl::swap(intersecPoints[0], intersecPoints[1]);
            }
            poly.push_back(intersecPoints[0]);
            poly.push_back(intersecPoints[1]);
        }
    }

    // EDGE 3
    if(pInside[7])
    {
        poly.push_back(q[3]);
        i8 intersectedEdge = -1;
        for(i8 iq = 0; iq < 4; iq++) {
            const u8 qs = qSlope(iq);
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            float xMin = a.x,
                  xMax = b.x;
            tl::minMax(xMin, xMax);
            if(s.xMin <= xMin || s.xMin >= xMax)
                continue;
            if(qs == 2) { // '-'
                if(a.y > s.yMin && a.y < s.yMax) {
                    poly.push_back({s.xMin, a.y});
                    intersectedEdge = iq;
                    break;
                }
            }
            else if(qs == 3 || qs == 4) { // '/' || '\'
                vec2 p;
                p.x = s.xMin;
                const float alpha = (p.x - a.x) / (b.x - b.y);
                p.y = glm::mix(a.y, b.y, alpha);
                if(p.y > s.yMin && p.y < s.yMax) {
                    poly.push_back(p);
                    intersectedEdge = iq;
                    break;
                }
            }
        }
        if(intersectedEdge != 1) {
            i8 iq = (intersectedEdge + 1) % 4;
            while(pInside[iq]) {
                poly.push_back(q[iq]);
                iq = (iq + 1) % 4;
            }
        }
    }
    else // !pInside[7]
    {
        vec2 intersecPoints[2];
        i8 numInter = 0;
        for(i8 iq = 0; iq < 4; iq++) {
            const vec2 a = q[iq];
            const vec2 b = q[(iq+1)%4];
            const u8 qs = qSlope(iq);
            if(qs == 0 || qs == 1) // '.' || '|'
                continue;
            float xMin = a.x,
                  xMax = b.x;
            tl::minMax(xMin, xMax);
            if(s.xMin > xMin && s.xMin < xMax) {
                vec2 p;
                p.x = s.xMin;
                const float alpha = (p.x - a.x) / (b.x - a.x);
                p.y = glm::mix(a.y, b.y, alpha);
                if(p.y > s.yMin && p.y < s.yMax) {
                    intersecPoints[numInter] = p;
                    numInter++;
                }
            }
        }
        if(numInter == 1) {
            poly.push_back(intersecPoints[0]);
        }
        else if(numInter == 2) {
            if(intersecPoints[0].y < intersecPoints[1].y) {
                tl::swap(intersecPoints[0], intersecPoints[1]);
            }
            poly.push_back(intersecPoints[0]);
            poly.push_back(intersecPoints[1]);
        }
    }

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
