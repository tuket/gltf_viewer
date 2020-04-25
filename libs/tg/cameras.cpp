#include "cameras.hpp"

#define GLM_FOCE_RADIANS
#include <glm/gtx/euler_angles.hpp>

namespace tg
{

glm::mat4 calcOrbitCameraMtx(float heading, float pitch, float distance)
{
    auto mtx = glm::eulerAngleXY(pitch, heading);
    mtx[3][2] = -distance;
    return mtx;
}

}
