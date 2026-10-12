// RERev2 - ReXGlue Recompiled Project

#include "generated/default/RERev2_init.h"

#include "RERev2_app.h"
#include "ppc_thunk_dispatcher.h"
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>

REX_DEFINE_APP(RERev2, Rerev2App::Create)

// Fast native path for MT Framework inline getter 0x82F57288
void sub_82F57288(PPCContext& ctx, uint8_t* base) {
  uint32_t r11 = 0x83AC0000;
  r11 = REX_LOAD_U32(r11 + 0x6C20);
  ctx.r11.u64 = r11;
  ctx.r3.u64 = REX_LOAD_U64(r11 + 0x28);
}

namespace rex::runtime {
PPCFunc* ResolveIndirectFunction(uint32_t guest_address) {
  if (guest_address == 0x82F57288) {
    return &sub_82F57288;
  }

  if (auto* rt = rex::Runtime::instance()) {
    if (auto* fd = rt->function_dispatcher()) {
      if (auto* fn = fd->GetFunction(guest_address)) {
        return fn;
      }
    }
  }

  return &ExecutePpcThunk;
}
}

