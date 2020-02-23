#include <glm/vec2.hpp>
#include <tl/carray.hpp>
#include <tl/rect.hpp>

bool segmentIntersect(glm::vec2& out, glm::vec2 a0, glm::vec2 a1, glm::vec2 b0, glm::vec2 b1);

bool isPointInsideRect(glm::vec2 p, const tl::rect& r);
bool isPointInsideQuad(glm::vec2 p, tl::CArray<glm::vec2>& q);

float triangleArea(glm::vec2 a, glm::vec2 b, glm::vec2 c);
float intersectionArea_square_quad(const tl::rect& s, tl::CArray<glm::vec2>& q);
