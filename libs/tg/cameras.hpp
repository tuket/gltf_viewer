#pragma once

#include <glm/matrix.hpp>

namespace tg
{

// takes radians
glm::mat4 calcOrbitCameraMtx(float heading, float pitch, float distance);

}
