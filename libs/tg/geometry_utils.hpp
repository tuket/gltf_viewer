#include <glm/vec2.hpp>
#include <tl/span.hpp>
#include <tl/rect.hpp>

namespace tg
{

bool isPointInsideRect(glm::vec2 p, const tl::rect& r);
bool isPointInsideQuad(glm::vec2 p, tl::CSpan<glm::vec2> q);

float triangleArea(glm::vec2 a, glm::vec2 b, glm::vec2 c);
float convexPolyArea(tl::CSpan<glm::vec2> poly);

/* Computes the area of the intersection between a asquare and a quad
 * The points of the quad must be given in counter-clock-wise order */
float intersectionArea_square_quad(const tl::rect& s, tl::CSpan<glm::vec2> q);

/* Given directed line defined by two points (s0, s1) and a point(a),
 * returns
 * -1: the point is on the left
 * 0: the point is on the line
 * +1: the point is on the right */
i8 calcPointSideWrtLine(glm::vec2 s0, glm::vec2 s1, glm::vec2 a);

// vertex positions of a cube center in the origin of corrdinates with side of length 2
extern const float cubeVerts[6*6*(3+2)];

}
