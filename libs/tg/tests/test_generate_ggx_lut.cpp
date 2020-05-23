#include "test_utils.hpp"
#include <tg/texture_utils.hpp>

bool test_generateGgxLut()
{
    simpleInitGlfwGL();
    tg::Img3f img = tg::generateGgxLutImg(512);
    const bool savedOk = img.save("ggx_lut.png");
    return savedOk;
}
