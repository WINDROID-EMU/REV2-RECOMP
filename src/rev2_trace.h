#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
__attribute__((visibility("default"))) void Rev2TraceHit(uint32_t addr);
#ifdef __cplusplus
}
#endif

void Rev2TraceDump();

#define REV2_TRACE(addr) do { if (Rev2TraceHit) Rev2TraceHit(addr); } while(0)
