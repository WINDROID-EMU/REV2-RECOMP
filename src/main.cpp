// RERev2 - ReXGlue Recompiled Project

#include "generated/default/RERev2_init.h"

#include "RERev2_app.h"
#include "ppc_thunk_dispatcher.h"
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>

REX_DEFINE_APP(RERev2, Rerev2App::Create)

namespace rex::runtime {
PPCFunc* ResolveIndirectFunction(uint32_t guest_address) {
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

