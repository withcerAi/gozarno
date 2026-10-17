#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define LWIP_RAND() ((uint32_t)gozarno_stack_random())
#ifdef __cplusplus
extern "C" {
#endif
uint32_t gozarno_stack_random(void);
#ifdef __cplusplus
}
#endif
#define LWIP_PLATFORM_DIAG(x) do { } while(0)
#define LWIP_PLATFORM_ASSERT(x) abort()
