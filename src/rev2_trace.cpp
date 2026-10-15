#include "rev2_trace.h"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <vector>

static constexpr uint32_t kBase = 0x834E0000, kEnd = 0x83520000;
static std::atomic<uint32_t> g_hits[(kEnd - kBase) / 4];
static std::atomic<uint32_t> g_swaps{0};

extern "C" __attribute__((visibility("default"))) void Rev2TraceHit(uint32_t a) {
  if (a < kBase || a >= kEnd) return;
  g_hits[(a - kBase) >> 2].fetch_add(1, std::memory_order_relaxed);
  if (a == 0x834F3F80 && (g_swaps.fetch_add(1) % 300) == 299) {
    Rev2TraceDump();
  }
}

void Rev2TraceDump() {
  std::vector<std::pair<uint32_t, uint32_t>> v;
  for (uint32_t i = 0; i < (kEnd - kBase) / 4; ++i) {
    uint32_t h = g_hits[i].exchange(0);
    if (h) v.push_back({h, kBase + i * 4});
  }
  std::sort(v.rbegin(), v.rend());
  std::printf("\n=======================================================\n");
  std::printf("   TOP D3D FUNCTIONS (Chamadas por 300 frames)\n");
  std::printf("=======================================================\n");
  for (size_t i = 0; i < v.size() && i < 60; ++i) {
    std::printf("0x%08X : %u chamadas\n", v[i].second, v[i].first);
  }
  std::printf("=======================================================\n\n");
  std::fflush(stdout);
}
