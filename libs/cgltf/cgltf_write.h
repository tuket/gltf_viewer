/**
 * cgltf_write - a single-file glTF 2.0 writer written in C99.
 *
 * Version: 1.4
 *
 * Website: https://github.com/jkuhlmann/cgltf
 *
 * Distributed under the MIT License, see notice at the end of this file.
 *
 * Building:
 * Include this file where you need the struct and function
 * declarations. Have exactly one source file where you define
 * `CGLTF_WRITE_IMPLEMENTATION` before including this file to get the
 * function definitions.
 *
 * Reference:
 * `cgltf_result cgltf_write_file(const cgltf_options* options, const char*
 * path, const cgltf_data* data)` writes JSON to the given file path. Buffer
 * files and external images are not written out. `data` is not deallocated.
 *
 * `cgltf_size cgltf_write(const cgltf_options* options, char* buffer,
 * cgltf_size size, const cgltf_data* data)` writes JSON into the given memory
 * buffer. Returns the number of bytes written to `buffer`, including a null
 * terminator. If buffer is null, returns the number of bytes that would have
 * been written. `data` is not deallocated.
 */
#pragma once

#include "cgltf.h"

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

cgltf_result cgltf_write_file(const cgltf_options* options, const char* path, const cgltf_data* data);
cgltf_size cgltf_write(const cgltf_options* options, char* buffer, cgltf_size size, const cgltf_data* data);

#ifdef __cplusplus
}
#endif
