#pragma once

#include <tl/str.hpp>
#include <cgltf.h>

bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f);

CStr cgltfPrimitiveTypeStr(cgltf_primitive_type type);