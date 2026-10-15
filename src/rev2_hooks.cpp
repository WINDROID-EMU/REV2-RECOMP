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
extern "C" void __imp__sub_835023E8(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E7708(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_83503180(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834F4980(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834F4AF0(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_83502F90(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E8E80(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_83503040(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834ECC48(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E7CB8(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E6BE8(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_83502050(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834F8650(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834F8278(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834F8AE0(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834E6E90(PPCContext& __restrict ctx, uint8_t* base);
extern "C" void __imp__sub_834ECE50(PPCContext& __restrict ctx, uint8_t* base);

// Inicialização automática no startup
struct AutoInitRev2Gpu {
  AutoInitRev2Gpu() {
    rev2::gpu::Initialize();
  }
} g_auto_init_rev2_gpu;

// 1. Present / Swap (0x834F3F80)
extern "C" void sub_834F3F80(PPCContext& __restrict ctx, uint8_t* base) {
  uint32_t device = ctx.r3.u32;
  rev2::gpu::OnPresent(ctx.r4.u32);
  if (base && device) {
    // Prevent the vsync lag spin-lock in loc_834F4684 (spins if 16968 - 16976 >= 15):
    uint32_t submitted = *reinterpret_cast<uint32_t*>(base + device + 16968);
    *reinterpret_cast<uint32_t*>(base + device + 16976) = submitted;
  }
  std::fprintf(stderr, "[REV2_HOOKS] sub_834F3F80: chamando __imp__ (device=0x%08X)...\n", device);
  __imp__sub_834F3F80(ctx, base);
  std::fprintf(stderr, "[REV2_HOOKS] sub_834F3F80: __imp__ retornou com sucesso!\n");
  if (base && device) {
    uint32_t submitted = *reinterpret_cast<uint32_t*>(base + device + 16968);
    *reinterpret_cast<uint32_t*>(base + device + 16976) = submitted;
  }
  ctx.r3.u64 = 0;
}

// 2. Clear (0x83501AC8)
extern "C" void sub_83501AC8(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnClear(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.f1.f32, ctx.r7.u32);
  __imp__sub_83501AC8(ctx, base);
}

// 3. SetRenderTarget (0x834E7610)
extern "C" void sub_834E7610(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetRenderTarget(ctx.r4.u32, ctx.r5.u32);
  __imp__sub_834E7610(ctx, base);
}

// 4. SetDepthStencilSurface (0x834E4480)
extern "C" void sub_834E4480(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetDepthStencilSurface(ctx.r4.u32);
  __imp__sub_834E4480(ctx, base);
}

// 5. SetViewport (0x83502290)
extern "C" void sub_83502290(PPCContext& __restrict ctx, uint8_t* base) {
  if (base && ctx.r4.u32) {
    const float* vp = reinterpret_cast<const float*>(base + ctx.r4.u32);
    rev2::gpu::OnSetViewport(vp[0], vp[1], vp[2], vp[3], vp[4], vp[5]);
  }
  __imp__sub_83502290(ctx, base);
}

// 6. SetVertexShaderConstantF (0x834EC978)
extern "C" void sub_834EC978(PPCContext& __restrict ctx, uint8_t* base) {
  if (base && ctx.r5.u32) {
    const float* pData = reinterpret_cast<const float*>(base + ctx.r5.u32);
    rev2::gpu::OnSetVSConstantF(ctx.r4.u32, pData, ctx.r6.u32);
  }
  __imp__sub_834EC978(ctx, base);
}

// 7. SetPixelShaderConstantF (0x834EC8A0)
extern "C" void sub_834EC8A0(PPCContext& __restrict ctx, uint8_t* base) {
  if (base && ctx.r5.u32) {
    const float* pData = reinterpret_cast<const float*>(base + ctx.r5.u32);
    rev2::gpu::OnSetPSConstantF(ctx.r4.u32, pData, ctx.r6.u32);
  }
  __imp__sub_834EC8A0(ctx, base);
}

// 8. SetTexture (0x834EADD8)
extern "C" void sub_834EADD8(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetTexture(ctx.r4.u32, ctx.r5.u32, base);
  __imp__sub_834EADD8(ctx, base);
}

// 9. DrawIndexedPrimitive (0x834E5060)
extern "C" void sub_834E5060(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnDrawIndexedPrimitive(ctx.r4.u32, ctx.r5.s32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32);
  rev2::gpu::EmitVkDrawIndexed(ctx.r4.u32, ctx.r5.s32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, base);
  __imp__sub_834E5060(ctx, base);
}

// 10. Resolve (0x834ED068)
extern "C" void sub_834ED068(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnResolve(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  __imp__sub_834ED068(ctx, base);
}

// 11. SetStreamSource (0x835023E8)
extern "C" void sub_835023E8(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetStreamSource(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, base);
  __imp__sub_835023E8(ctx, base);
}

// 12. SetIndices (0x834E7708)
extern "C" void sub_834E7708(PPCContext& __restrict ctx, uint8_t* base) {
  uint32_t ib_addr = (ctx.r5.u32 != 0) ? ctx.r5.u32 : ctx.r4.u32;
  rev2::gpu::OnSetIndices(ib_addr, base);
  __imp__sub_834E7708(ctx, base);
}

// 13. SetVertexDeclaration (0x83503180)
extern "C" void sub_83503180(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetVertexDeclaration(ctx.r4.u32);
  __imp__sub_83503180(ctx, base);
}

// 14. SetVertexShader (0x834F4980)
extern "C" void sub_834F4980(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetVertexShader(ctx.r4.u32);
  __imp__sub_834F4980(ctx, base);
}

// 14b. SetPixelShader (0x834F4AF0)
extern "C" void sub_834F4AF0(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetPixelShader(ctx.r4.u32);
  __imp__sub_834F4AF0(ctx, base);
}

// 15. SetRenderState (0x83502F90)
extern "C" void sub_83502F90(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetRenderState(ctx.r4.u32, ctx.r5.u32);
  __imp__sub_83502F90(ctx, base);
}

// 16. SetSamplerState (0x834E8E80)
extern "C" void sub_834E8E80(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetSamplerState(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  __imp__sub_834E8E80(ctx, base);
}

// 17. SetScissorRect (0x83503040)
extern "C" void sub_83503040(PPCContext& __restrict ctx, uint8_t* base) {
  if (base && ctx.r4.u32) {
    const int32_t* rect = reinterpret_cast<const int32_t*>(base + ctx.r4.u32);
    rev2::gpu::OnSetScissorRect(rect[0], rect[1], rect[2], rect[3]);
  }
  __imp__sub_83503040(ctx, base);
}

// 18. DrawPrimitive (0x834ECC48)
extern "C" void sub_834ECC48(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnDrawPrimitive(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  rev2::gpu::EmitVkDrawPrimitive(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, base);
  __imp__sub_834ECC48(ctx, base);
}

// 19. BeginTiling (0x834E7CB8)
extern "C" void sub_834E7CB8(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnBeginTiling(ctx.r4.u32);
  __imp__sub_834E7CB8(ctx, base);
}

// 20. EndTiling (0x834E6BE8)
extern "C" void sub_834E6BE8(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnEndTiling();
  __imp__sub_834E6BE8(ctx, base);
}

// 21. FlushDirtyRegisters (0x83502050)
extern "C" void sub_83502050(PPCContext& __restrict ctx, uint8_t* base) {
  __imp__sub_83502050(ctx, base);
}

// 22. SetBlendState (0x834F8650)
extern "C" void sub_834F8650(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetBlendState(ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
  __imp__sub_834F8650(ctx, base);
}

// 23. SetDepthStencilState (0x834F8278)
extern "C" void sub_834F8278(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetDepthStencilState(ctx.r4.u32, ctx.r5.u32);
  __imp__sub_834F8278(ctx, base);
}

// 24. SetAlphaTestState (0x834F8AE0)
extern "C" void sub_834F8AE0(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetAlphaTestState(ctx.r4.u32, ctx.r5.u32);
  __imp__sub_834F8AE0(ctx, base);
}

// 25. SetColorWriteMask (0x834E6E90)
extern "C" void sub_834E6E90(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnSetColorWriteMask(ctx.r4.u32, ctx.r5.u32);
  __imp__sub_834E6E90(ctx, base);
}

// 26. ResolveParameters (0x834ECE50)
extern "C" void sub_834ECE50(PPCContext& __restrict ctx, uint8_t* base) {
  rev2::gpu::OnResolveParameters(ctx.r4.u32);
  __imp__sub_834ECE50(ctx, base);
}
