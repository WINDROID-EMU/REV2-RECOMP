#include <rex/hook.h>
#include <rex/ppc/func.h>
#include "rev2_gpu/rev2_gpu.h"
#include <cstdio>
#include <atomic>

// Declarações das funções originais do código recompilado
extern "C" void __imp__sub_834F3F80(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_83501AC8(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E7610(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E4480(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_83502290(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834EC978(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834EC8A0(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834EADD8(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E5060(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834ED068(PPCContext& __restrict ctx, uint8_t* base);

// Inicialização automática no startup
struct AutoInitRev2Gpu {
  AutoInitRev2Gpu() {
    rev2::gpu::Initialize();
  }
} g_auto_init_rev2_gpu;

// 1. Present / Swap (0x834F3F80)
extern "C" void sub_834F3F80(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnPresent(ctx.r4.u32);
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834F3F80(ctx, base);
  } else {
    // Apresentação direta via Plume / host presenter
    __imp__sub_834F3F80(ctx, base); // mantemos swap de sincronismo do presenter
    ctx.r3.u64 = 0;
  }
}

// 2. Clear (0x83501AC8)
extern "C" void sub_83501AC8(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnClear(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.f1.f32, ctx.r7.u32);
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_83501AC8(ctx, base);
  } else {
    ctx.r3.u64 = 0; // S_OK / D3D_OK sem submissão legada ao ring buffer Xenos
  }
}

// 3. SetRenderTarget (0x834E7610)
extern "C" void sub_834E7610(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetRenderTarget(ctx.r4.u32, ctx.r5.u32);
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834E7610(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}

// 4. SetDepthStencilSurface (0x834E4480)
extern "C" void sub_834E4480(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetDepthStencilSurface(ctx.r4.u32);
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834E4480(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}

// 5. SetViewport (0x83502290)
extern "C" void sub_83502290(PPCContext& __restrict ctx, uint8_t* base) {
  if (base && ctx.r4.u32) {
    const float* vp = reinterpret_cast<const float*>(base + ctx.r4.u32);
    rev2::gpu::OnSetViewport(vp[0], vp[1], vp[2], vp[3], vp[4], vp[5]);
  }
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_83502290(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}

// 6. SetVertexShaderConstantF (0x834EC978)
extern "C" void sub_834EC978(PPCContext& __restrict ctx, uint8_t* base) {
  if (base && ctx.r5.u32) {
    const float* pData = reinterpret_cast<const float*>(base + ctx.r5.u32);
    rev2::gpu::OnSetVSConstantF(ctx.r4.u32, pData, ctx.r6.u32);
  }
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834EC978(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}

// 7. SetPixelShaderConstantF (0x834EC8A0)
extern "C" void sub_834EC8A0(PPCContext& __restrict ctx, uint8_t* base) {
  if (base && ctx.r5.u32) {
    const float* pData = reinterpret_cast<const float*>(base + ctx.r5.u32);
    rev2::gpu::OnSetPSConstantF(ctx.r4.u32, pData, ctx.r6.u32);
  }
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834EC8A0(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}

// 8. SetTexture (0x834EADD8)
extern "C" void sub_834EADD8(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetTexture(ctx.r4.u32, ctx.r5.u32, base);
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834EADD8(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}

// 9. DrawIndexedPrimitive (0x834E5060)
extern "C" void sub_834E5060(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnDrawIndexedPrimitive(ctx.r4.u32, ctx.r5.s32, ctx.r6.u32, ctx.r7.u32, 0, 0);
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834E5060(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}

// 10. Resolve (0x834ED068)
extern "C" void sub_834ED068(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnResolve(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  if (rev2::gpu::PipelineState::Instance().fallback_to_xenos) {
    __imp__sub_834ED068(ctx, base);
  } else {
    ctx.r3.u64 = 0;
  }
}
