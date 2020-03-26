#include "geometry_utils.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <tl/containers/fvector.hpp>
#include <tl/basic_math.hpp>
#include <tl/basic.hpp>

using glm::vec2;

int segmentsIntersect(vec2 (&out)[2], vec2 a0, vec2 a1, vec2 b0, vec2 b1)
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
    return abs(0.5f * dot(v1p, v2));
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

float intersectionArea_square_quad(const tl::rect& s, tl::CSpan<vec2> q)
{
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
    auto nextEdge = [](int e) {
        if(e < 4)
            return (e+1) % 4;
        else
            return 4 + (e-4+1) % 4;
    };
    auto approxEqual = [](vec2 a, vec2 b) {
        return glm::distance2(a, b) < 0.0001f;
    };
    /*auto segmentVsQuadIntersect = [&](int& outEdge, vec2 &outPoint, int inEdge) -> bool
    {
        vec2 a, b;
        const vec2* oq; //< other quad
        if(inEdge < 4) {
            a = sp[inEdge];
            b = sp[(inEdge+1)%4];
            oq = q;
            outEdge = 4;
        }
        else {
            a = q[inEdge-4];
            b = q[(inEdge-4+1)%4];
            oq = sp;
            outEdge = 0;
        }
        for(int i = 0; i < 4; i++)
        if(segmentsIntersect(outPoint, a, b, oq[i], oq[(i+1)%4])) {
            outEdge += i;
            return true;
        }
        return false;
    };*/
    auto segmentVsQuadIntersect2 = [&](int (&outEdges)[2], vec2 (&outPoints)[2], int inEdge) -> int
    {
        vec2 a, b;
        const vec2* oq; //< other quad
        if(inEdge < 4) {
            a = sp[inEdge];
            b = sp[(inEdge+1)%4];
            oq = q;
            outEdges[0] = outEdges[1] = 4;
        }
        else {
            a = q[inEdge-4];
            b = q[(inEdge-4+1)%4];
            oq = sp;
            outEdges[0] = outEdges[1] = 0;
        }
        int np = 0;
        for(int i = 0; i < 4; i++)
        if(segmentIntersect(outPoints[np], a, b, oq[i], oq[(i+1)%4])) {
            outEdges[np] = i;
            np++;
        }
        if(np == 2)
        {
            const float d0 = glm::distance2(a, outPoints[0]);
            const float d1 = glm::distance2(a, outPoints[1]);
            if(d0 > d1) {
                tl::swap(outEdges[0], outEdges[1]);
                tl::swap(outPoints[0], outPoints[1]);
            }
            else if(fabs(d1 - d0) < 0.0001f)
                np--;
        }
        return np;
    };
    const bool pInside[8] = {
        isPointInsideRect(q[0], s),
        isPointInsideRect(q[1], s),
        isPointInsideRect(q[2], s),
        isPointInsideRect(q[3], s),
        isPointInsideQuad(sp[0], q),
        isPointInsideQuad(sp[1], q),
        isPointInsideQuad(sp[2], q),
        isPointInsideQuad(sp[3], q)
    };
    if(pInside[0] && pInside[1] && pInside[2] && pInside[3]) { // square inside quad
        const float w = s.pMax.x - s.pMin.x;
        return w*w;
    }
    if(pInside[4] && pInside[5] && pInside[6] && pInside[7]) { // quad inside square
        return triangleArea(q[0], q[1], q[2]) + triangleArea(q[2], q[3], q[0]);
    }

    tl::FVector<vec2, 8> poly; // here we compute the intersection polygon
    auto addVert = [&](vec2 p) {
        if(!approxEqual(poly.back(), p))
            poly.push_back(p);
    };
    bool prevWasInside = false;
    if(pInside[0]) {
        poly.push_back(q[0]);
        prevWasInside = true;
    }
    int curEdge = 0;
    while(poly.size() < 8)
    {
        int numInserted = 1;
        if(prevWasInside)
        {
            int intersectedEdges[2];
            vec2 intersectionPoints[2];
            const int numIntersectionPoints = segmentVsQuadIntersect2(intersectedEdges, intersectionPoints, curEdge);
            if(numIntersectionPoints) {
                for(int i = 0; i < numIntersectionPoints; i++)
                    poly.push_back(intersectionPoints[i]);
                prevWasInside = false;
                curEdge = intersectedEdges[1];
            }
            else {
                poly.push_back(edgePos1(curEdge));
                curEdge = nextEdge(curEdge);
            }
        }
        else // !prevWasInside
        {
            int intersectedEdges[2];
            vec2 intersectionPoints[2];
            int numIntersections = segmentVsQuadIntersect2(intersectedEdges, intersectionPoints, curEdge);
            if(numIntersections == 0) {
                poly.push_back(edgePos1(curEdge));
                curEdge = nextEdge(curEdge);
            }
            else if(numIntersections == 1) {
                poly.push_back(intersectionPoints[0]);
                curEdge = intersectedEdges[0];
                prevWasInside = true;
            }
            else { // numIntersections == 2
                poly.push_back(intersectionPoints[0]);
                poly.push_back(intersectionPoints[1]);
                curEdge = intersectedEdges[1];
                numInserted = 2;
            }
        }
        if(poly.size() >= 3) {
            if(approxEqual(poly[0], poly.back()))
                break;
            if(numInserted == 2 && approxEqual(poly[0], poly[poly.size()-2])) {
                poly.pop_back();
                break;
            }
        }
    }
    const float area = convexPolyArea(poly);
    return area;
}
