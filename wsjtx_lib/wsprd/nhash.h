#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t nhash(const void* key, size_t length, uint32_t initval);

#ifdef __cplusplus
}
#endif
