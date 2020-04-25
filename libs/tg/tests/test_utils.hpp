#pragma once

#include <glm/mat3x3.hpp>

struct OrbitCamData {
    float heading, pitch, distance;
    glm::mat3 getViewMat()const;
};
