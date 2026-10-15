#pragma once
#include <cstdint>

extern "C" void Rev2TraceHit(uint32_t addr);
void Rev2TraceDump();

#define REV2_TRACE(addr) Rev2TraceHit(addr)
