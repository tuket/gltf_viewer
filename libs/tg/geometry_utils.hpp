#include <glm/vec2.hpp>
#include <tl/span.hpp>
#include <tl/rect.hpp>


/* computes the intersection point(s) of two segments
 * Returns 0 if they don't intersect
 * Returns 1 if the segments intersect, the intersection points is returned in out1
 * Returns 2 if the segements lie on the same line, in out1 and out2 you will get the points of the *intersection segment* (if the intersection is at one point, 1 will be returned)
*/
int segmentsIntersect(glm::vec2 (&out)[2], glm::vec2 a0, glm::vec2 a1, glm::vec2 b0, glm::vec2 b1);

bool isPointInsideRect(glm::vec2 p, const tl::rect& r);
bool isPointInsideQuad(glm::vec2 p, tl::CSpan<glm::vec2> q);

float triangleArea(glm::vec2 a, glm::vec2 b, glm::vec2 c);
float convexPolyArea(tl::CSpan<glm::vec2> poly);
float intersectionArea_square_quad(const tl::rect& s, tl::CSpan<glm::vec2> q);
